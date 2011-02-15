#! /usr/bin/env python

import numpy as np
import numpy.ma as ma
import re

NCFG = 21*2
re_result = r'(\S+) (\S+):((?:\s+[<>X]\d+){42}) +[0-9-]+ c(\d+)'
re_score = r'\s*([<>X])(\d+)'

# parse the results report into a scorecard

t = open('results.txt')
results = list(t)
t.close()

progs = {}
scorekey = { '<': 1, '>': -1, 'X': 0 }

r = re.compile(re_result)
r2 = re.compile(re_score)

for l in results:
    m = r.match(l)
    if m is None: continue

    p1 = m.group(1)
    p2 = m.group(2)
    cc = float(m.group(4))/NCFG

    prog1 = progs.setdefault(p1, {})
    prog2 = progs.setdefault(p2, {})

    s = []
    for spec in m.group(3).split():
        m2 = r2.match(spec)
        if m2 is None: raise Exception('bad spec')
        s.append((scorekey[m2.group(1)], int(m2.group(2))))
    if len(s) != NCFG:
        raise Exception('bad number of configs')

    prog1[p2] = (s, cc)
    prog2[p1] = ([(-p, c) for p, c in s], cc)

# assign numeric ids for progs

proglist = progs.keys()
proglist.sort()

nprogs = len(proglist)
progmap = dict((prog, pid) for pid, prog in enumerate(proglist))

# generate the (masked) array representation for scores and duel points

scores = np.nan * np.zeros((nprogs, NCFG*nprogs))
cycles = np.nan * np.zeros((nprogs, NCFG*nprogs))

dpoints = np.nan * np.zeros((nprogs, nprogs))
dcycles = np.nan * np.zeros((nprogs, nprogs))

for row, prog in enumerate(proglist):
    for opponent, (s, cc) in progs[prog].iteritems():
        col = progmap[opponent]

        scores[row, col*NCFG:(col+1)*NCFG] = [p for p, c in s]
        cycles[row, col*NCFG:(col+1)*NCFG] = [c for p, c in s]
        dpoints[row, col] = np.array(s).sum()
        dcycles[row, col] = cc

scores = ma.masked_invalid(scores)
cycles = ma.masked_invalid(cycles)
dpoints = ma.masked_invalid(dpoints)
dcycles = ma.masked_invalid(dcycles)

# compute overall points and tournament scores

points = dpoints.sum(1) / NCFG
points = points.filled(np.nan) # should be no nans

w = (points+nprogs)/((nprogs-1)*2) # compatible with the officiality
w.shape = (1, nprogs)

t = dpoints.filled(0)
t[t < 0] = 0
tscores = (t * w).sum(1) / NCFG * 200 / (nprogs-1)

# construct "predicted" scores by a similarity-weighted averaging

pscores = scores.filled(0)

dist = np.zeros((nprogs, nprogs))

for i in xrange(0, nprogs-1):
    for j in xrange(i+1, nprogs):
        d = abs(scores[i,:] - scores[j,:]).sum()
        dist[i,j] = d
        dist[j,i] = d

dist[dist == 0] = 0.1

for i in xrange(0, nprogs):
    others = np.array(xrange(0, nprogs)) != i
    w = 1/dist[others, i:i+1]
    p = (pscores[others, i*21*2:(i+1)*21*2] * w).sum(0) / w.sum()
    pscores[i, i*21*2:(i+1)*21*2] = p

# dump results for further processing

def simplify_name(p):
    if p.endswith('.bfjoust'):
        p = p[:-8]
    t = p.rfind('/')
    if t != -1:
        p = p[t+1:]
    return p

proglist = [simplify_name(p) for p in proglist]

np.savez('results.npz',
         points=points,
         tscores=tscores,
         scores=scores.filled(np.nan),
         pscores=pscores,
         dpoints=dpoints.filled(np.nan),
         cycles=cycles.filled(np.nan),
         dcycles=dcycles.filled(np.nan),
         proglist=proglist)
