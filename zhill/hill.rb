require 'json'
require 'yaml'

require_relative 'gear'
require_relative 'score'

# Hill combat logic:
#
# Hill consists of N programs.  When a new challenger is submitted,
# the currently worst-ranking program is replaced by the challenger.
# If the program name exactly matches an existing program, that one is
# replaced instead.  Then the duel points and rankings are recomputed.

# Hill object fields:
# @dir: hill repository directory
# @progs: dict name -> progobj
# @hill: list name -- in order of progobj['id']
# @results: hash name -> hash name -> list int -- only for (A, B) pairs where A['id'] < B['id']
# @ranking: list name -- in reverse order of score
# @gear: gearlanced instance running the hill
# @max_score: maximum theoretical score under current scoring method
# @tapecount: total number of tape configurations on this hill
# @scoring: current scoring mechanism
#
# progobj:
#   'id': integer index in @hill and in gear
#   'name': program canonical name
#   'points': sum of duel points for scoring
#   'score': current score under the scoring mechanic
#   'rank': current index in @ranking
#   'prevrank': previous index (if any) before the latest challenge
#   'file': stored source file (unless a pseudo-program)

class Hill
  include Enumerable
  attr_reader :dir, :cfg, :tapecount, :max_score

  def initialize(hilldir)
    @dir = File.absolute_path(hilldir)

    # verify that hill directory is a Git repo

    @commit = git_HEAD

    # load configuration

    @cfg = YAML.load_file(File.join(@dir, 'hill.yaml'))
    hillcfg = @cfg['hill']

    nprogs = hillcfg['size'].to_i
    @scoring = Scoring::Methods[hillcfg['scoring']]
    @gear = Gear.new(nprogs, hillcfg['gearlanced'])
    @tapecount = hillcfg['tapecount'].to_i
    @tiebreak = hillcfg['tiebreak']

    # construct the initial hill with initial results

    @progs = {}
    @hill = []
    @results = {}

    progfiles = Dir[File.join(@dir, '*.bfjoust')]

    (0 .. nprogs-1).each do |id|
      # prepare program

      p = { 'id' => id }
      code = nil

      if id < progfiles.length
        p['file'] = progfiles[id]
        p['name'] = File.basename(p['file'], '.bfjoust')
        File.open(p['file']) { |f| code = f.read }
      else
        p['name'] = "dummy-#{id}"
        code = '<'
      end

      prog = p['name']

      # insert into data structures

      @progs[prog] = p
      @hill << prog
      @results[prog] = {}

      # test against previous programs to update results

      if id > 0
        result = @gear.test(code)
        (0 .. id-1).each do |idA|
          progA = @hill[idA]
          @results[progA][prog] = result[idA].map { |r| -r }
        end
      end

      # put into gearlanced

      @gear.set(id, code)
    end

    # compute initial scores

    @max_score = @scoring.max(self)
    rescore

    @progs.each_value { |p| p['prevrank'] = p['rank'] }
  end

  # implement a set-like abstraction for the program names

  def length; @hill.length; end
  def size; @hill.length; end
  def each(&block); @hill.each(&block); end
  def [](id); @hill[id]; end

  def include?(prog); @progs.key?(prog); end
  def id(prog); @progs[prog]['id']; end

  def pairs
    n = @hill.length
    (0 .. n-2).each do |idA|
      (idA+1 .. n-1).each do |idB|
        yield @hill[idA], @hill[idB]
      end
    end
  end

  def id_pairs
    n = @hill.length
    (0 .. n-2).each do |idA|
      (idA+1 .. n-1).each do |idB|
        yield idA, @hill[idA], idB, @hill[idB]
      end
    end
  end

  # accessors for raw results and duel points, as well as current ranking

  def results(progA, progB)
    progA = @progs[progA]
    progB = @progs[progB]

    return [0] * (2*@tapecount) if progA['id'] == progB['id']

    scale = 1
    scale, progA, progB = -1, progB, progA if progA['id'] > progB['id']

    @results[progA['name']][progB['name']].map { |p| scale*p }
  end

  def points(prog)
    @progs[prog]['points']
  end

  def ranking
    @ranking.each_with_index do |prog, rank|
      p = @progs[prog]
      yield rank, prog, p['points'], p['score']
    end
  end

  # hill program file management

  def save(replaced, newprog, code)
    newfile = File.join(@dir, newprog + '.bfjoust')

    if replaced.key?('file') && replaced['file'] != newfile
      git('rm', '-q', '--', replaced['file'])
      replaced['file'] = nil
    end

    File.open(newfile, 'w') { |f| f.write(code) }
    git('add', '--', newfile)

    commitmsg = nil
    if replaced['name'] == newprog
      commitmsg = "Updating #{newprog}"
    else
      commitmsg = "Replacing #{replaced['name']} by #{newprog}"
    end

    if git_changes?
      git('commit', '-q', '-m', commitmsg)
      git('push', @cfg['push'], 'master') if @cfg['push']
      @commit = git_HEAD
    end

    newfile
  end
  private :save

  # a new challenger appears (or an old one is changed)

  def challenge(newprog, code)
    # select a program to replace

    replaced = @progs[newprog] || @progs[@ranking[-1]]
    newid = replaced['id']

    # test for parse errors, compute scores, replace old program

    result = @gear.test(code)
    @gear.set(newid, code)

    # replace old program with new in hill data structures

    newp = { 'id' => newid, 'name' => newprog }
    newp['rank'] = replaced['rank'] if replaced['name'] == newprog

    @progs.delete(replaced['name'])
    @progs[newprog] = newp

    @hill[newid] = newprog

    # remove old results

    @results.delete(replaced['name'])
    @hill[0,newid].each { |progA| @results[progA].delete(replaced['name']) }

    # insert in new results

    @results[newprog] = {}

    result.each_with_index do |oppres, oppid|
      opp = @hill[oppid]
      if oppid < newid
        @results[opp][newprog] = oppres.map { |p| -p }
      elsif newid < oppid
        @results[newprog][opp] = oppres
      end
    end

    # recompute points and ranking

    rescore

    # update source repository

    newp['file'] = save(replaced, newprog, code)

    # return summary information about the new program

    rankchg = ''
    if newp.key?('prevrank')
      delta = newp['prevrank'] - newp['rank']
      rankchg = " (change: #{delta == 0 ? '--' : '%+d' % delta})"
    end

    '%s: points %.2f, score %.2f/%s, rank %d/%d%s' %
      [newprog,
       newp['points'],
       newp['score'], @max_score,
       newp['rank'] + 1, @progs.length, rankchg]
  end

  def rescore
    n = @progs.length

    @progs.each_value do |p|
      p['prevrank'] = p['rank'] if p.key?('rank')
    end

    # recompute points for all programs

    points = [0] * n

    id_pairs do |idA, progA, idB, progB|
      s = @results[progA][progB].reduce(:+)
      points[idA] += s
      points[idB] -= s
    end

    points.each_with_index { |p, i| @progs[@hill[i]]['points'] = p.to_f / (2*@tapecount) }

    # recompute scores

    scores = @scoring.score(self)
    @progs.each { |prog, p| p['score'] = scores[prog] }

    # recompute ranking

    @ranking = @hill.sort { |a, b| @progs[b]['score'] <=> @progs[a]['score'] }
    @ranking.each_with_index { |prog, r| @progs[prog]['rank'] = r }
  end
  private :rescore

  # reports

  def write_report(out)
    n = @progs.length

    sym = ->(r) { r < 0 ? '-' : r > 0 ? '+' : '0' }
    chg = lambda do |p|
      if p.key?('prevrank')
        delta = p['prevrank'] - p['rank']
        delta == 0 ? ' --' : '%+3d' % delta
      else
        'new'
      end
    end

    out.write("Rank/chg   Score  Points  Program\n")
    ranking do |rank, prog, points, score|
      out.write("%4d %s %7.2f %7.2f  %s\n" % [rank+1, chg.call(@progs[prog]), score, points, prog])
    end
    out.write("\n")

    out.write('   | ' +
              (1..n).map { |i| i/10 > 0 ? (i/10).to_s : ' ' }.join(' ') +
              " |\n")
    out.write('   | ' +
              (1..n).map { |i| i%10 }.join(' ') +
              " | score | pts   | program\n")
    ranking do |rank, prog, points, score|
      out.write("%2d | %s |%6.1f |%6.1f | %s\n" %
                [rank + 1,
                 @ranking.map { |opp| opp != prog ? sym.call(results(prog, opp).reduce(:+)) : ' ' }.join(' '),
                 score, points, prog])
    end
  end

  def write_breakdown(prog, out)
    sym = { -1 => '>', 0 => 'X', +1 => '<' }
    t = @tapecount

    @hill.sort.each do |opp|
      next if opp == prog

      res = results(prog, opp)
      tot = res.reduce(:+)

      out.write("#{prog} vs #{opp}\n")
      out.write(res[0,t].map { |r| sym[r] }.join +
                ' ' +
                res[t,t].map { |r| sym[r] }.join +
                " #{tot}\n")
      out.write(tot == 0 ? "Tie.\n" : "#{tot > 0 ? prog : opp} wins.\n")
    end
  end

  def write_json(out, varname=nil)
    n = @ranking.length

    data = {
      'commit' => @commit,
      'progs' => @ranking,
      'prevrank' => @ranking.map { |p| @progs[p]['prevrank'] },
      'results' => (0..n-2).map do |a|
        (a+1..n-1).map { |b| results(@ranking[a], @ranking[b]) }
      end
    }

    scores = {}
    Scoring::Methods.each do |name, m|
      s = m.score(self)
      scores[name] = {
        'max' => m.max(self).to_f,
        'score' => @ranking.map { |p| s[p] }
      }
    end
    data['scores'] = scores

    out.write("var #{varname}=") unless varname.nil?
    JSON.dump(data, out)
    out.write(";\n") unless varname.nil?
  end

  # internal helper methods for git use

  def git(*args)
    Dir.chdir(@dir) do
      git = Process.spawn('git', *args)
      Process.wait(git)
      raise "git #{args.join(' ')} (in #{@dir}) failed: #{$?.exitstatus}" if $?.exitstatus != 0
    end
  end
  private :git

  def git_changes?
    Dir.chdir(@dir) do
      git = Process.spawn('git', 'diff-index', '--cached', '--quiet', 'HEAD')
      Process.wait(git)
      raise "git diff-index failed weirdly: #{$?.exitstatus}" if $?.exitstatus > 1
      $?.exitstatus == 1
    end
  end
  private :git_changes?

  def git_HEAD
    Dir.chdir(@dir) do
      commit = `git rev-parse HEAD`
      raise "git rev-parse HEAD failed: #{$?.exitstatus}" if $?.exitstatus != 0
      commit.chomp
    end
  end
  private :git_HEAD
end
