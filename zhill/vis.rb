require 'json'

require_relative 'gear'
require_relative 'score'

# This file contains the tools for generating pre-aggregated JSON
# statistics for use by zhill website visualizations.

module Vis
  GLOBAL = 0
  PER_PROGRAM = 1

  Outputs = {}

  def Vis.generate(stats, out_dir, what)
    # update shared metadata json file

    markov = Scoring::Methods['markov']
    score = markov.score(HillAdapter.new(stats))

    # TODO: maxcycles from the hill
    File.open(File.join(out_dir, 'meta.js'), 'w') do |f|
      f.write('var Vis = ')
      JSON.dump({ :N => stats.N, :T => stats.T,
                  :mintape => stats.mintape, :maxtape => stats.maxtape,
                  :maxcycles => 100000,
                  :names => stats.names,
                  :scores => stats.names.map { |p| score[p] } },
                f)
      f.write(";\n")
    end

    # generate all visualizations

    if what.empty?
      what = Outputs.keys.sort
    else
      what = what.map do |v|
        v = v.to_sym
        raise RuntimeError, "unknown visualization: #{v}" unless Outputs.include?(v)
        v
      end
    end

    puts "generating: #{what.join(' ')}"

    what.each do |vname|
      v = Outputs[vname]
      out_prefix = File.join(out_dir, "#{vname}")

      print "#{vname}:"; STDOUT.flush

      if v.type == GLOBAL
        print ' all'; STDOUT.flush
        data = v.generate(stats)
        File.open("#{out_prefix}.json", 'w') { |f| JSON.dump(data, f) }
      elsif v.type == PER_PROGRAM
        (0 ... stats.N).each do |p|
          print " #{p}"; STDOUT.flush
          data = v.generate(stats, p)
          File.open("#{out_prefix}.p#{p}.json", 'w') { |f| JSON.dump(data, f) }
        end
      else
        raise RuntimeError, "unknown visualization type: #{v.type}"
      end

      puts ''
    end
  end
end

# Pre-aggregated outputs that can be generated:
# =============================================

module Vis
  # Superclass for tape value/heatmap plots.
  class TapePlot
    def type; Vis::PER_PROGRAM; end
    def generate(stats, left)
      all = Array.new(stats.N) { |right| data(stats, left, right) }
      avg = Vis.a_sum(all)
      { :all => all, :avg => avg }
    end
    def data(stats, left, right)
      raise RuntimeError, 'TapePlot.data'
    end
  end

  # prog_heat_position:
  # Per-program tape plot. Heatmap for the tape pointer position.
  class ProgHeatPosition < TapePlot
    def data(stats, left, right)
      stats.heat_position(left, right).map { |t| t[0 ... t.length/2] }
    end
  end
  Outputs[:prog_heat_position] = ProgHeatPosition.new

  # prog_tape_abs:
  # Per-program tape plot. Absolute values on tape at the end of
  # match.
  class ProgTapeAbs < TapePlot
    def data(stats, left, right)
      stats.tape_abs(left, right).map do |tape|
        tape.bytes.map { |v| v > 128 ? 256 - v : v }
      end
    end
  end
  Outputs[:prog_tape_abs] = ProgTapeAbs.new
end

# General-purpose utility code:
# =============================

module Vis
  # Function for cranking a hill. This generates the upper triangular
  # (including the diagonal) data for playing each distinct pair of
  # opponents against each other. While the points on the diagonal are
  # invariant to the program (all ties), other statistics do depend on
  # the program in question.
  def Vis.crank(bin, progs)
    nprogs = progs.length
    gear = Gear.new(nprogs, bin)

    data = Array.new(nprogs) do |i|
      p = File.read(progs[i])
      gear.set(i, p)
      gear.test(p)[0..i]
    end

    names = progs.map { |f| File.basename(f, '.bfjoust') }

    [names, data]
  end

  def Vis.a_sum(arrs)
    n = arrs.length
    Array.new(arrs[0].length) do |i|
      items = Array.new(n) { |j| arrs[j][i] }
      items[0].class == Array ?
        a_sum(items) :
        items.inject(:+)
    end
  end

  def Vis.a_mul(arr, s)
    arr.map do |v|
      v.class == Array ? Vis.a_mul(v, s) : v*s
    end
  end
end

# Utility class to abstract away the mirrored nature of the
# statistics, and dynamically generate the requested statistics from
# the point of view of any program.
class Stats
  attr_reader(:names, :N)
  attr_reader(:mintape, :maxtape, :T, :T2)

  # Construct a new Stats object given the #crank results.
  def initialize(names, data)
    @names = names
    @N = names.length

    @data = data

    @mintape = data[0][0][:stats][0][:tape_abs].length
    @maxtape = data[0][0][:stats][-1][:tape_abs].length
    @T = data[0][0][:points].length / 2
    @T2 = 2 * @T

    @idata = Array.new(@N - 1) { |i| Array.new(i) }
  end

  # Public statistics accessors.

  def total_points(left, right)
    get(left, right)[:points].inject(:+)
  end

  def total_cycles(left, right)
    get(left, right)[:cycles]
  end

  def points(left, right, tape=nil, pol=nil)
    points = get(left, right)[:points]
    if tape.nil?
      points
    else
      points[tape + (pol ? @T : 0)]
    end
  end

  [:cycles, :tape_abs, :tape_max, :heat_position].each do |key|
    define_method(key) do |left, right, tape=nil, pol=nil|
      if tape.nil?
        (0...@T2).map { |t| self.send(key, left, right, t) }
      else
        get(left, right, tape, pol)[key]
      end
    end
  end

  # Private helpers.

  # Retrieve statistics for any pair +left+, +right+. If raw data does
  # not exist for the pair, synthetic data will be generated and
  # cached. The optional arguments +tape+, +pol+ can be used to
  # extract a single object from the :stats key, given a tape length
  # (0 .. @T-1) and polarity (true, false). Raw indices (0 .. @T2-1)
  # can be used by not specifying the polarity.
  def get(left, right, tape=nil, pol=nil)
    if left < 0 or left >= @N or right < 0 or right >= @N
      raise RuntimeError, "access (#{left}, #{right}) outside bounds"
    end

    d =
      if left <= right
        @data[right][left]
      else  # right < left
        flipped = @idata[left - 1][right]
        if flipped.nil?
          flipped = flip(@data[left][right])
          @idata[left - 1][right] = flipped
        end
        flipped
      end

    if tape.nil?
      d
    else
      d[:stats][tape + (pol ? @T : 0)]
    end
  end
  private :get

  # Given a statistics object, return the statistics as seen from the
  # point of view of the opponent.
  def flip(d)
    {
      :points => d[:points].map { |p| -p },
      :cycles => d[:cycles],
      :stats => d[:stats].map do |s|
        tlen = s[:tape_abs].length
        {
          :cycles => s[:cycles],
          :tape_abs => s[:tape_abs].reverse,
          :tape_max => s[:tape_max].reverse,
          :heat_position => s[:heat_position].reverse,
        }
      end,
    }
  end
  private :flip
end

# Provide a Hill-like adapter to a Stats object, so that the shared
# Markov score computation score can act on it.
class HillAdapter
  include Enumerable

  def initialize(stats)
    @stats = stats
    @names = Hash[@stats.names.each_with_index.to_a]
  end

  def tapecount; @stats.T; end

  def [](idx); @stats.names[idx]; end
  def size; @stats.N; end
  alias_method :length, :size

  def each(&block) # :yields: name
    @stats.names.each(&block)
  end

  def id_pairs # :yields: idA, progA, idB, progB
    n = @stats.N
    (0 .. n-2).each do |idA|
      (idA+1 .. n-1).each do |idB|
        yield idA, @stats.names[idA], idB, @stats.names[idB]
      end
    end
  end

  def results(progA, progB)
    @stats.points(@names[progB], @names[progA])
  end
end
