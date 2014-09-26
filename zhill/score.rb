require 'nmatrix'

module Scoring
  # "points" method: sum of raw duel points, moved to range [0, 2*(N-1)]

  class Points
    def score(hill)
      n = hill.size
      Hash[hill.map { |p| [p, hill.points(p) + n - 1] }]
    end

    def max(hill)
      (2 * (hill.size - 1)).to_s
    end
  end

  # "markov" method: stationary distribution of a Markov chain

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
  #
  # simple fix: normalize to [0, 100] by scaling with max score

  class Markov
    def score(hill)
      n = hill.size
      k = 2 * hill.tapecount

      t = NMatrix.zeros(n)
      hill.id_pairs do |idA, progA, idB, progB|
        results = hill.results(progA, progB)
        lossAB = results.count { |r| r < 0 }
        lossBA = results.count { |r| r > 0 }
        t[idA,idB] = lossAB.to_f / (n * k)
        t[idB,idA] = lossBA.to_f / (n * k)
      end

      t += NMatrix.diagonal(-t.sum(1) + 1) # make rows sum to 1

      # speed up converging by squaring t a couple of times
      (1..5).each { t = t.dot(t) }
      # converge
      d, pd = NMatrix.ones([1,n]) / n, nil
      (1..100).each do |step|
        pd = d
        d = d.dot(t)
        break if (d-pd).abs.mean(1).to_f < 0.000001
      end

      #d /= d.max(1).to_f
      d *= 1000

      Hash[hill.each_with_index.map { |p, i| [p, d[0,i]] }]
    end

    def max(_); '1000'; end
  end

  # "trad" method: conventional egojoust scoring

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
    def initialize(weightf)
      @weightf = weightf
    end

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

    def max(_); '100'; end
  end

  # "iterative" method: fixed point of a trad-inspired scoring scheme

  # see zem.fi/tmp/bfjoust.html for now

  class Iter
    def initialize(weightf)
      @weightf = weightf
    end

    def score(hill)
      n = hill.size
      k = 2 * hill.tapecount

      # pre-compute "positive scaled i-vs-j duel points" matrix D
      dp = NMatrix.zeros(n)
      hill.id_pairs do |idA, progA, idB, progB|
        t = hill.results(progA, progB).reduce(:+)
        if t > 0
          dp[idA,idB] = @weightf.call(t.to_f, k.to_f)
        else
          dp[idB,idA] = @weightf.call(-t.to_f, k.to_f)
        end
      end

      # initialize base score from raw points
      s = NMatrix.zeros([n, 1])
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

    def max(_); '100'; end
  end

  # all known scoring methods

  Methods = {
    'points' => Points.new,
    'markov' => Markov.new,
    'trad' => Trad.new(->(dp, k) { dp/k }),
    'tradtwist' => Trad.new(->(dp, k) { (dp+k)/(2*k) }),
    'iter' => Iter.new(->(dp, k) { dp/k }),
    'itertwist' => Iter.new(->(dp, k) { (dp+k)/(2*k) })
  }
end
