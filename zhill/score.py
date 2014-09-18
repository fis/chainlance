import numpy as np

# "points" method: sum of raw duel points, moved to range [0, 2*(N-1)]

def points_score(hill):
    n = len(hill)
    return dict((p, hill.points(p) + (n-1)) for p in hill)

def points_max(hill):
    return '{0:d}'.format(2 * (len(hill) - 1))

# "trad" method: conventional egojoust scoring

# N = number of programs, M = N-1, K = maximum duel points (2*tapecount)
# worth of p = (points of p + N) / (M*2)
# base score of p:
#   for each program i != p:
#     if p's duel points against i > 0:
#       p's base score += i's worth * p-vs-i duel points / K
# score of p = (base * 200) / M

def trad_score(hill):
    n = len(hill)
    k = 2 * hill.tapecount()

    worth = dict((p, (hill.points(p) + n) / (2 * (n-1))) for p in hill)

    base = dict((p, 0) for p in hill)
    for progA, progB in hill.pairs():
        dp = sum(hill.results(progA, progB))
        # tweak:   dp/k => (dp+k)/(2*k)   (-dp)/k => ((-dp)+k)/(2*k)
        if dp > 0:
            base[progA] += worth[progB] * dp / k
        elif dp < 0:
            base[progB] += worth[progA] * (-dp) / k

    return dict((p, 200 * b / (n-1)) for p, b in base.items())

# "iterative" method: fixed point of a trad-inspired scoring scheme

# score^(0) = average of raw duel points, scaled to [0,1] possible range
# score^(i):
#   ... preferrably something that preserves sum(score) ...
#   for program p:
#     for each opponent o != p:
#       ...

def iter_score(hill):
    n = len(hill)
    k = 2 * hill.tapecount()

    # pre compute "positive scaled i-vs-j duel points" matrix
    dp = np.zeros((n,n))
    for idA, progA, idB, progB in hill.idpairs():
        t = sum(hill.results(progA, progB))
        if t > 0:
            dp[idA,idB] = t / k
        elif t < 0:
            dp[idB,idA] = (-t) / k

    # initialize base score from raw points
    s = np.zeros((n,1))
    for i, prog in enumerate(hill):
        s[i,0] = (hill.points(prog) + (n-1)) / (2 * (n-1))

    # iterate until convergence, or for maximum number of steps
    for step in range(100):
        ps = s
        s = np.dot(dp, ps)
        s = s / s.sum() * (n/2)
        if abs(s - ps).mean() < 0.000001:
            break

    return dict((p, 100*s[i,0]) for i, p in enumerate(hill))

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

def markov_score(hill):
    n = len(hill)
    k = 2 * hill.tapecount()

    t = np.zeros((n,n))
    for idA, progA, idB, progB in hill.idpairs():
        results = hill.results(progA, progB)
        lossAB = sum(1 for r in results if r == -1)
        lossBA = sum(1 for r in results if r == 1)
        t[idA,idB] = lossAB / (n * k)
        t[idB,idA] = lossBA / (n * k)

    for i in range(n):
        t[i,i] = 1 - t[i,:].sum()

    # speed up converging by squaring t a couple of times
    for step in range(5):
        t = np.dot(t, t)
    # converge
    d = np.ones((1,n)) / n
    for step in range(100): # maximum number of steps
        pd = d
        d = np.dot(d, t)
        if abs(d-pd).mean() < 0.000001:
            break

    d /= d.max()
    d *= 100

    return dict((p, d[0,i]) for i, p in enumerate(hill))

# all known scoring methods

scorings = {
    'points': {
        'score': points_score,
        'max': points_max
    },
    'trad': {
        'score': trad_score,
        'max': lambda h: '100'
    },
    'iter': {
        'score': iter_score,
        'max': lambda h: '100'
    },
    'markov': {
        'score': markov_score,
        'max': lambda h: '100'
    }
}
