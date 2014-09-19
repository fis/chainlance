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

      d /= d.max(1).to_f
      d *= 100

      Hash[hill.each_with_index.map { |p, i| [p, d[0,i]] }]
    end

    def max(_); '100'; end
  end

  # all known scoring methods

  Methods = {
    'points' => Points.new,
    'markov' => Markov.new
  }
end
