require 'numo/narray'

# BF Joust scoring methods module.
#
# For details on the scoring methods, see {zem.fi hill internals}[http://zem.fi/bfjoust/internals/].

module Scoring
  # The "points" method: sum of raw duel points, moved to range [0, 2*(N-1)].
  class Points
    # Scores for a Hill.
    def score(hill)
      n = hill.size
      Hash[hill.map { |p| [p, hill.points(p) + n - 1] }]
    end

    # Theoretical maximum score for a Hill.
    def max(hill)
      (2 * (hill.size - 1)).to_s
    end
  end

  # The "markov" method: stationary distribution of a Markov chain.
  #--
  # let there be a (first-order) Markov chain, with the transition
  # probabilities defined as
  #
  #  t(a,b) = |{losses of a against b}| / (N*K)
  #  t(a,a) = 1 - sum_{b!=a} t(a,b)
  #
  # let score be the stationary distribution. what happens?
  #
  # - an entirely losing program will have a score of 0: good
  # - an entirely winning program may have a small score: bad
  #   *** in retrospect: not true; it will have a score of 1 ***
  #
  # simple fix: normalize to [0, 100] by scaling with max score
  # better fix: just multiply by 1000
  class Markov
    # Scores for a Hill.
    def score(hill)
      n = hill.size
      k = 2 * hill.tapecount

      t = Numo::DFloat.zeros(n, n)
      hill.id_pairs do |idA, progA, idB, progB|
        results = hill.results(progA, progB)
        lossAB = results.count { |r| r < 0 }
        lossBA = results.count { |r| r > 0 }
        t[idA,idB] = lossAB.to_f / (n * k)
        t[idB,idA] = lossBA.to_f / (n * k)
      end

      tsum = t.sum(axis: 1)
      t[Numo::NArray.diag_indices(n, n)] += Numo::DFloat.ones(tsum.shape) - tsum # make rows sum to 1

      # speed up converging by squaring t a couple of times
      (1..5).each { t = t.dot(t) }
      # converge
      d, pd = Numo::DFloat.ones(1, n) / n, nil
      (1..100).each do |step|
        pd = d
        d = d.dot(t)
        break if (d-pd).abs.mean(axis: 1).to_f < 0.000001
      end

      #d /= d.max(1).to_f
      d *= 1000

      Hash[hill.each_with_index.map { |p, i| [p, d[0,i]] }]
    end

    # Theoretical maximum score for a Hill (constant).
    def max(_); '1000'; end
  end

  # The "trad" method: conventional egojoust scoring.
  #--
  # N = number of programs, M = N-1, K = maximum duel points (2*tapecount)
  # worth of p = (points of p + N) / (M*2)
  # base score of p:
  #   for each program i != p:
  #     if p's duel points against i > 0:
  #       p's base score += i's worth * p-vs-i duel points / K
  # score of p = (base * 200) / M
  #
  # as a tweak to reward narrow wins more,
  # the D/K term can be replaced with (D+K)/(2K)
  class Trad
    # Construct a new scorer with a given weight function.
    def initialize(weightf)
      @weightf = weightf
    end

    # Scores for a Hill.
    def score(hill)
      n = hill.size
      k = 2 * hill.tapecount

      worth = hill.map { |p| (hill.points(p) + n) / (2 * (n-1)) }

      base = [0] * n
      hill.id_pairs do |idA, progA, idB, progB|
        dp = hill.results(progA, progB).reduce(:+)
        if dp > 0
          base[idA] += worth[idB] * @weightf.call(dp.to_f, k.to_f)
        elsif dp < 0
          base[idB] += worth[idA] * @weightf.call(-dp.to_f, k.to_f)
        end
      end

      Hash[hill.each_with_index.map { |p, i| [p, 200 * base[i] / (n-1)] }]
    end

    # Theoretical maximum score for a Hill (constant).
    def max(_); '100'; end
  end

  # The "iterative" method: fixed point of a trad-inspired scoring scheme.
  #--
  # see http://zem.fi/bfjoust/internals/ for now
  class Iter
    # Construct a new scorer with a given weight function.
    def initialize(weightf)
      @weightf = weightf
    end

    # Scores for a Hill.
    def score(hill)
      n = hill.size
      k = 2 * hill.tapecount

      # pre-compute "positive scaled i-vs-j duel points" matrix D
      dp = Numo::DFloat.zeros(n, n)
      hill.id_pairs do |idA, progA, idB, progB|
        t = hill.results(progA, progB).reduce(:+)
        if t > 0
          dp[idA,idB] = @weightf.call(t.to_f, k.to_f)
        else
          dp[idB,idA] = @weightf.call(-t.to_f, k.to_f)
        end
      end

      # initialize base score from raw points
      s = Numo::DFloat.zeros(n, 1)
      hill.each_with_index { |p, i| s[i,0] = (hill.points(p) + (n-1)) / (2 * (n-1)) }

      # iterate until convergence, or for maximum number of steps
      (1 .. 100).each do |_|
        ps = s
        s = dp.dot(ps)
        s = s / s.sum.to_f * (n/2)
        break if (s - ps).abs.mean.to_f < 0.000001
      end

      Hash[hill.each_with_index.map { |p, i| [p, 100 * s[i,0]] }]
    end

    # Theoretical maximum score for a Hill (constant).
    def max(_); '100'; end
  end

  # A hash of all known scoring methods.
  #
  # The keys of the hash are scoring method names:
  # [points]     Scoring::Points
  # [markov]     Scoring::Markov
  # [trad]       Scoring::Trad (regular mode)
  # [tradtwist]  Scoring::Trad (tweaked mode)
  # [iter]       Scoring::Iter (regular mode)
  # [itertwist]  Scoring::Iter (tweaked mode)
  #
  # Each method object accepts the messages +max+ (maximum theoretical
  # score) and +score+ (scores for the programs in a Hill object).

  Methods = {
    'points' => Points.new,
    'markov' => Markov.new,
    'trad' => Trad.new(->(dp, k) { dp/k }),
    'tradtwist' => Trad.new(->(dp, k) { (dp+k)/(2*k) }),
    'iter' => Iter.new(->(dp, k) { dp/k }),
    'itertwist' => Iter.new(->(dp, k) { (dp+k)/(2*k) })
  }
end
