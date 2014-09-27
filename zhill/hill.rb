# -*- coding: utf-8 -*-
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

# BF Joust hill manager.
#
# This class is responsible for high-level management of a BF Joust
# hill, as physically represented by a hill Git repository directory.

class HillManager
  # Absolute path to the hill directory.
  attr_reader :dir
  # Parsed YAML configuration file for the hill.
  attr_reader :cfg
  # A Hill object containing the current programs and their scores.
  attr_reader :hill

  # Constructs a new hill manager for a repository at +hilldir+.
  #
  # Configuration will be read from the file +hilldir/hill.yaml+.  The
  # +hill+ section has the following known configuration keys.
  #
  # [size]        Size of the hill, in number of programs.
  # [scoring]     One of the scoring method names from the Scoring module.
  # [gearlanced]  Path to the +gearlanced+ tool for jousting.
  # [tapecount]   Number of tape lengths, normally always 21.
  # [tiebreak]    *TODO* Secret key for tie-breaking.
  # [push]        Optional; name of a remote repository to push after each commit.
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

  # Computes the results and ranking of a new challenger.
  #
  # +newprog+ is the name of the new program, and +code+ its source
  # code.  This method will replace one program on the hill: either an
  # old version, if a program named +newprog+ currently exists, or
  # otherwise the last-ranked program.  If +try_only+ is false, the
  # current hill is permanently updated.  In any case, a single-line
  # summary message about the new program's ranking (in the modified
  # hill) will be returned.
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

  # Permanently saves the source code of a new program.
  #
  # +replaced+ must be the name of a program currently on the hill;
  # +newprog+ will be used as the basis of the new program's source
  # file name, and +code+ will be written there.
  def save(replaced, newprog, code)
    oldfile = @files[replaced]
    newfile = File.join(@dir, newprog + '.bfjoust')

    # remove old file (if not a dummy program) unless replacing it

    unless oldfile.nil? || oldfile == newfile
      git('rm', '-q', '--', oldfile)
      @files.delete(replaced)
    end
    @files[newprog] = newfile

    # store new code and register for commit

    File.open(newfile, 'w') { |f| f.write(code) }
    git('add', '--', newfile)

    # if any changes were made, commit and maybe push

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

  # Runs a Git command with the given arguments.
  def git(*args)
    Dir.chdir(@dir) do
      git = Process.spawn('git', *args)
      Process.wait(git)
      raise "git #{args.join(' ')} (in #{@dir}) failed: #{$?.exitstatus}" if $?.exitstatus != 0
    end
  end
  private :git

  # Tests whether the repository directory has changes for a commit.
  def git_changes?
    Dir.chdir(@dir) do
      git = Process.spawn('git', 'diff-index', '--cached', '--quiet', 'HEAD')
      Process.wait(git)
      raise "git diff-index failed weirdly: #{$?.exitstatus}" if $?.exitstatus > 1
      $?.exitstatus == 1
    end
  end
  private :git_changes?

  # Returns the commit hash of the current HEAD.
  def git_HEAD
    Dir.chdir(@dir) do
      commit = `git rev-parse HEAD`
      raise "git rev-parse HEAD failed: #{$?.exitstatus}" if $?.exitstatus != 0
      commit.chomp
    end
  end
  private :git_HEAD

  # Writes a plaintext report.
  #
  # +out+ must be an IO object to write the report to.  The formatting
  # of the report follows the traditional style.
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

  # Writes a plaintext breakdown file.
  #
  # The breakdown will be written from the perspective of program
  # +prog+, into IO object +out+.  The style is traditional.
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

  # Writes a JSON report of current hill state.
  #
  # The report will be written to IO object +out+.  If +varname+ is
  # not +nil+, an assignment to +varname+ will be written around the
  # JSON object, so that the end result can be directly included as a
  # JavaScript file.
  #
  # The JSON object will have the following properties:
  #
  # [commit]         Commit hash for the hill represented by the report.
  # [progs]          List of program names, ordered by rank (best first).
  # [prevrank]       Array of "old" rankings for each program.
  # [results]        Results for each joust; see below.
  # [scores]         Scores according to all methods; see below.
  # [scoringMethod]  Name of the scoring method in use.
  #
  # If there are _N_ programs on the hill, the *results* property is
  # an array of length _N_-1, and the element _i_ (from 0 .. _N_-2) is
  # itself an array of length _N_-1-_i_.  The element
  # *results*(_i_,_j_) contains the results of the match between
  # programs _i_ (as the left program) and _i_+1+_j_ (as the right
  # program), where those numbers are indices into *progs*.  Each such
  # element is (yet another) array of length 2*_T_, where _T_ is the
  # number of tape lengths, containing +1, 0 or -1 depending on
  # whether the _left_ program wins, ties or loses the corresponding
  # configuration, respectively.
  #
  # The *scores* property is an object, with one property named after
  # each known scoring method.  The values of these properties are
  # again objects, containing two properties: *max* (theoretical
  # maximum score) and *score* (an array of scores, in the same order
  # as *progs*).
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

# A BF Joust hill, tracking programs and their scores.
#
# This class maintains a set of programs and the results of a BF Joust
# battle between any pair of programs.  It also handles the updating
# of those results when one program is replaced.

class Hill
  include Enumerable
  # Number of different tape lengths, normally 21 (for 10 .. 30).
  attr_reader :tapecount

  # Constructs a new hill.
  #
  # *cfg* should be the +hill+ section from a hill YAML configuration
  # file.  For the list of keys, see HillManager::new.
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

  # Number of programs in this hill.
  def size; @hill.length; end
  alias_method :length, :size
  # Enumerates the program names in hill index order.
  def each(&block) # :yields: name
    @hill.each(&block)
  end
  # Returns a program name corresponding to a hill index.
  def [](id); @hill[id]; end

  # Checks whether program named +prog+ exists on the hill.
  def include?(prog); @progs.key?(prog); end
  # Returns the hill index of program named +prog+.
  def id(prog); @progs[prog]; end

  # Enumerates all pairs of program names in hill index order.
  #
  # Order: (0, 1), (0, 2), ..., (0, _N_), (1, 2), (1, 3), ...,
  # (1, _N_), ..., (_N_-1, _N_).
  def pairs # :yields: progA, progB
    n = @hill.length
    (0 .. n-2).each do |idA|
      (idA+1 .. n-1).each do |idB|
        yield @hill[idA], @hill[idB]
      end
    end
  end

  # Enumerates all pairs of programs in the same order as #pairs.
  def id_pairs # :yields: idA, progA, idB, progB
    n = @hill.length
    (0 .. n-2).each do |idA|
      (idA+1 .. n-1).each do |idB|
        yield idA, @hill[idA], idB, @hill[idB]
      end
    end
  end

  # Replace a program and return a new, modified hill.
  #
  # The program named +replaced+ will be replaced by a program named
  # +newprog+, where +results+ must be the results of that program
  # against the current hill, as determined by Gear#test.
  def replace(replaced, newprog, results)
    new_hill = dup
    new_hill.replace!(replaced, newprog, results)
    new_hill
  end
  # Internal parts of the #replace method to construct the modifed hill.
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

  # Results of the battle between +progA+ and +progB+.
  def results(progA, progB); @results[@progs[progA], @progs[progB]]; end
  # Duel points for program +prog+.
  def points(prog); @points[@progs[prog]]; end
  # Score (under current scoring method) for program +prog+.
  def score(prog); @scores[prog]; end

  # Current rank of program +prog+.
  def rank(prog); @ranks[prog]; end
  # Previous rank of program +prog+.
  def oldrank(prog); @oldranks[prog]; end
  # Checks if program +prog+ existed in the previous revision of the hill.
  def oldrank?(prog); @oldranks.include?(prog); end

  # Enumerates the hill programs ordered by rank.
  #
  # The yielded value +rank+ is the 0-based ranking.
  #
  # If a block is not provided, returns an array of program names
  # ordered by ranking (best first).
  def ranking # :yields: rank, prog, points, score
    return @ranking unless block_given?
    @ranking.each_with_index do |prog, rank|
      yield rank, prog, @points[@progs[prog]], @scores[prog]
    end
  end
end

# Efficiently stored matrix of results.
#
# This class stores the (conceptual) _N_ × _N_ matrix of results by
# only keeping the upper triangular part (without the main diagonal),
# as the other half can be reconstructed from symmetry, and the
# diagonal is all zeros.

class ResultMatrix
  # Size of the matrix.
  attr_reader :size

  # Constructs a new result matrix.
  #
  # The matrix will be of size +size+, and each element will have
  # 2*+tapecount+ entries.  (This is used to synthesize the zero results
  # on the diagonal, when requested.)
  def initialize(size, tapecount)
    @size = size
    @tapecount = tapecount

    @data = Array.new(size-1) { |i| Array.new(size-1 - i) }
  end

  # Returns the results for pair (+a+, +b+).
  def [](a, b)
    return [0] * (2*@tapecount) if a == b
    if a < b
      @data[a][b-a - 1]
    else
      @data[b][a-b - 1].map { |r| -r }
    end
  end

  # Updates the results of battle (+a+, +b+).
  #
  # The results of (+b+, +a+) will automatically reflect this change,
  # and an attempt to modify (+a+, +a+) is silently ignored.
  def []=(a, b, results)
    return if a == b

    if a < b
      @data[a][b-a - 1] = results
    else
      @data[b][a-b - 1] = results.map { |r| -r }
    end
  end

  # Returns a new result matrix with the results of one program replaced.
  #
  # +idA+ is the index of the program being replaced, and +results+
  # the results (as returned by Gear#test) of it battling all the
  # other programs in the matrix.
  def replace(idA, results)
    new_matrix = dup
    new_matrix.replace!(idA, results)
    new_matrix
  end
  # Internal parts of the replace method to construct the modified matrix.
  def replace!(idA, results)
    results.each_with_index do |r, idB|
      self[idA, idB] = r
    end
  end
  protected :replace!
end
