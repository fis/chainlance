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

# OUTDATED STUFF
#
# Hill object fields:
# @dir: hill repository directory
# @progs: dict name -> progobj
# @hill: list name -- in order of progobj['id']
# @results: JoustResults -- current stored results
# @ranking: list name -- in reverse order of score
# @gear: gearlanced instance running the hill
# @max_score: maximum theoretical score under current scoring method
# @tapecount: total number of tape configurations on this hill
# @scoring: current scoring mechanism
#
# progobj:
#   :id - integer index in @hill and in gear
#   :name - program canonical name
#   :points - sum of duel points for scoring
#   :score - current score under the scoring mechanic
#   :rank - current index in @ranking
#   :file - stored source file (unless a pseudo-program)

class HillManager
  attr_reader :dir, :cfg, :hill

  def initialize(hilldir)
    @dir = File.absolute_path(hilldir)

    # verify that hill directory is a Git repo

    @commit = git_HEAD

    # load configuration

    @cfg = YAML.load_file(File.join(@dir, 'hill.yaml'))

    hillcfg = @cfg['hill']
    nprogs = hillcfg['size'].to_i

    # construct the initial hill with initial results

    @hill = Hill.new(hillcfg)
    @gear = Gear.new(nprogs, hillcfg['gearlanced'])

    progfiles = Dir[File.join(@dir, '*.bfjoust')]
    raise "too many programs: #{progfiles.size} > #{nprogs}" if progfiles.size > nprogs

    @files = {}

    progfiles.each_with_index do |file, id|
      name = File.basename(file, '.bfjoust')
      code = File.open(file) { |f| f.read }

      results = @gear.test(code)
      @hill = @hill.replace(@hill[id], name, results)
      @gear.set(id, code)

      @files[name] = file
    end
  end

  # a new challenger appears (or an old one is changed)

  def challenge(newprog, code, try_only=false)
    # make sure program parses and compute match results

    results = @gear.test(code)

    # select a program to replace

    replaced = @hill.include?(newprog) ? newprog : @hill.ranking[-1]
    newid = @hill.id(replaced)

    # construct the changed hill

    new_hill = @hill.replace(replaced, newprog, results)

    # make the changes permanent, if necessary

    unless try_only
      @hill = new_hill

      @gear.set(newid, code)
      save(replaced, newprog, code)
    end

    # construct the summary message

    rankchg = ''
    if new_hill.oldrank?(newprog)
      delta = new_hill.oldrank(newprog) - new_hill.rank(newprog)
      rankchg = " (#{delta == 0 ? '--' : '%+d' % delta})"
    end

    '%s: points %.2f, score %.2f, rank %d/%d%s' %
      [newprog,
       new_hill.points(newprog),
       new_hill.score(newprog),
       new_hill.rank(newprog) + 1, new_hill.size, rankchg]
  end

  # hill source file management

  def save(replaced, newprog, code)
    oldfile = @files[replaced]
    newfile = File.join(@dir, newprog + '.bfjoust')

    unless oldfile.nil? || oldfile == newfile
      git('rm', '-q', '--', oldfile)
      @files.delete(replaced)
    end
    @files[newprog] = newfile

    File.open(newfile, 'w') { |f| f.write(code) }
    git('add', '--', newfile)

    commitmsg = nil
    if replaced == newprog
      commitmsg = "Updating #{newprog}"
    else
      commitmsg = "Replacing #{replaced} by #{newprog}"
    end

    if git_changes?
      git('commit', '-q', '-m', commitmsg)
      git('push', '-q', @cfg['hill']['push'], 'master') if @cfg['hill']['push']
      @commit = git_HEAD
    end
  end
  private :save

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

  # reports

  def write_report(out)
    n = @hill.size

    sym = ->(r) { r < 0 ? '-' : r > 0 ? '+' : '0' }
    chg = lambda do |p|
      if @hill.oldrank?(p)
        delta = @hill.oldrank(p) - @hill.rank(p)
        delta == 0 ? ' --' : '%+3d' % delta
      else
        'new'
      end
    end

    out.write("Rank/chg   Score  Points  Program\n")
    @hill.ranking do |rank, prog, points, score|
      out.write("%4d %s %7.2f %7.2f  %s\n" % [rank+1, chg.call(prog), score, points, prog])
    end
    out.write("\n")

    out.write('   | ' +
              (1..n).map { |i| i/10 > 0 ? (i/10).to_s : ' ' }.join(' ') +
              " |\n")
    out.write('   | ' +
              (1..n).map { |i| i%10 }.join(' ') +
              " | score | pts   | program\n")
    @hill.ranking do |rank, prog, points, score|
      out.write("%2d | %s |%6.1f |%6.1f | %s\n" %
                [rank + 1,
                 @hill.ranking.map { |opp| opp != prog ? sym.call(@hill.results(prog, opp).reduce(:+)) : ' ' }.join(' '),
                 score, points, prog])
    end
  end

  def write_breakdown(prog, out)
    sym = { -1 => '>', 0 => 'X', +1 => '<' }
    t = @hill.tapecount

    @hill.sort.each do |opp|
      next if opp == prog

      res = @hill.results(prog, opp)
      tot = res.reduce(:+)

      out.write("#{prog} vs #{opp}\n")
      out.write(res[0,t].map { |r| sym[r] }.join +
                ' ' +
                res[t,t].map { |r| sym[r] }.join +
                " #{tot}\n")
      out.write(tot == 0 ? "Tie.\n" : "#{tot > 0 ? prog : opp} wins.\n")
      out.write("\n")
    end
  end

  def write_json(out, varname=nil)
    n = @hill.size

    progs = @hill.ranking

    data = {
      'commit' => @commit,
      'progs' => progs,
      'prevrank' => progs.map { |p| @hill.oldrank(p) },
      'results' => (0..n-2).map do |a|
        (a+1..n-1).map { |b| @hill.results(progs[a], progs[b]) }
      end
    }

    scores = {}
    Scoring::Methods.each do |name, m|
      s = m.score(@hill)
      scores[name] = {
        'max' => m.max(@hill).to_f,
        'score' => progs.map { |p| s[p] }
      }
    end
    data['scores'] = scores
    data['scoringMethod'] = @cfg['hill']['scoring']

    out.write("var #{varname}=") unless varname.nil?
    JSON.dump(data, out)
    out.write(";\n") unless varname.nil?
  end
end

class Hill
  include Enumerable
  attr_reader :tapecount, :results

  def initialize(cfg)
    @scoring = Scoring::Methods[cfg['scoring']]
    @tapecount = cfg['tapecount'].to_i
    @tiebreak = cfg['tiebreak'] # TODO: use

    # construct a hill of dummy programs

    nprogs = cfg['size'].to_i

    @progs = {}
    @hill = Array.new(nprogs)

    (0 .. nprogs-1).each do |id|
      prog = "dummy-#{id}"
      @progs[prog] = id
      @hill[id] = prog
    end

    # construct initial dummy results

    @results = ResultMatrix.new(nprogs, @tapecount)
    (0 .. nprogs-2).each do |idA|
      (idA+1 .. nprogs-1).each { |idB| @results[idA, idB] = Array.new(2*@tapecount, 0) }
    end
  end

  # implement a set-like abstraction for the program names

  def length; @hill.length; end
  def size; @hill.length; end
  def each(&block); @hill.each(&block); end
  def [](id); @hill[id]; end

  def include?(prog); @progs.key?(prog); end
  def id(prog); @progs[prog]; end

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

  # replace a program on the hill

  def replace(replaced, newprog, results)
    new_hill = dup
    new_hill.replace!(replaced, newprog, results)
    new_hill
  end

  def replace!(replaced, newprog, results)
    # replace the program in the hill data structures

    newid = @progs[replaced]

    @progs = @progs.dup
    @progs.delete(replaced)
    @progs[newprog] = newid

    @hill = @hill.dup
    @hill[newid] = newprog

    # replace the old program's results

    @results = @results.replace(newid, results)

    # recompute points

    n = @progs.length
    @points = [0] * n

    id_pairs do |idA, _, idB, _|
      s = @results[idA, idB].reduce(:+)
      @points[idA] += s
      @points[idB] -= s
    end

    @points.map! { |p| p.to_f / (2*@tapecount) }

    # recompute scores and rankings

    @scores = @scoring.score(self)
    @ranking = @hill.sort { |a, b| @scores[b] <=> @scores[a] }

    @oldranks = @ranks
    @ranks = Hash[@ranking.each_with_index.map { |p, r| [p, r] }]
  end
  protected :replace!

  # accessors to raw results, points, scores, ranking

  def results(progA, progB); @results[@progs[progA], @progs[progB]]; end
  def points(prog); @points[@progs[prog]]; end
  def score(prog); @scores[prog]; end

  def rank(prog); @ranks[prog]; end
  def oldrank(prog); @oldranks[prog]; end
  def oldrank?(prog); @oldranks.include?(prog); end

  def ranking
    return @ranking unless block_given?
    @ranking.each_with_index do |prog, rank|
      yield rank, prog, @points[@progs[prog]], @scores[prog]
    end
  end
end

class ResultMatrix
  attr_reader :size

  def initialize(size, tapecount)
    @size = size
    @tapecount = tapecount

    @data = Array.new(size-1) { |i| Array.new(size-1 - i) }
  end

  def [](a, b)
    return [0] * (2*@tapecount) if a == b
    if a < b
      @data[a][b-a - 1]
    else
      @data[b][a-b - 1].map { |r| -r }
    end
  end

  def []=(a, b, results)
    return if a == b

    if a < b
      @data[a][b-a - 1] = results
    else
      @data[b][a-b - 1] = results.map { |r| -r }
    end
  end

  def replace(idA, results)
    new_matrix = dup
    new_matrix.replace!(idA, results)
    new_matrix
  end

  def replace!(idA, results)
    results.each_with_index do |r, idB|
      self[idA, idB] = r
    end
  end
  protected :replace!
end
