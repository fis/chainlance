#! /usr/bin/env python

import cPickle as pickle
import re
from itertools import chain

import numpy as np
import numpy.ma as ma

MINTAPE = 10
MAXTAPE = 30

re_header = r'^! (\S+) (\S+)'
re_summary = r'^SUMMARY: ([<>X]{21}) ([<>X]{21}) [0-9-]+ c(\d+) sl(\d+) sr(\d+)'
re_cycles = r'^CYCLES\[(\d+),(\d+)\] = (\d+)'
re_tapeabs = r'^TAPEABS\[(\d+),(\d+)\] = ([-0-9 ]+)'
re_tapemax = r'^TAPEMAX\[(\d+),(\d+)\] = ([0-9/ ]+)'

NTAPES = MAXTAPE - MINTAPE + 1
NCFG = 2*NTAPES

# parse the results report into Python structures

t = open('results.txt')
results = list(t)
t.close()

progs = {}
proginfo = {}
scorekey = { '<': 1, '>': -1, 'X': 0 }

re_header = re.compile(re_header)
re_summary = re.compile(re_summary)
re_cycles = re.compile(re_cycles)
re_tapeabs = re.compile(re_tapeabs)
re_tapemax = re.compile(re_tapemax)

current_l, current_li = None, None
current_r, current_ri = None, None

def simplify_name(p):
    if p.endswith('.bfjoust'):
        p = p[:-8]
    t = p.rfind('/')
    if t != -1:
        p = p[t+1:]
    return p

for l in results:
    m = re_header.match(l)
    if m is not None:
        pln = simplify_name(m.group(1))
        prn = simplify_name(m.group(2))

        pl = progs.setdefault(pln, {})
        pr = progs.setdefault(prn, {})

        current_l = { 'cfg': ([{} for x in xrange(NTAPES)], [{} for x in xrange(NTAPES)]) }
        current_r = { 'cfg': ([{} for x in xrange(NTAPES)], [{} for x in xrange(NTAPES)]) }

        pl[prn] = current_l
        pr[pln] = current_r

        current_li = proginfo.setdefault(pln, {})
        current_ri = proginfo.setdefault(prn, {})

        continue

    m = re_summary.match(l)
    if m is not None:
        s = m.group(1) + m.group(2)
        current_l['score'] = [ scorekey[c] for c in s]
        current_r['score'] = [-scorekey[c] for c in s]
        current_l['cycles'] = current_r['cycles'] = int(m.group(3))
        current_li['len'] = int(m.group(4))
        current_ri['len'] = int(m.group(5))
        continue

    m = re_cycles.match(l)
    if m is not None:
        p = int(m.group(1))
        cfg = int(m.group(2)) - MINTAPE
        c = int(m.group(3))
        current_l['cfg'][p][cfg]['cycles'] = c
        current_r['cfg'][p][cfg]['cycles'] = c
        continue

    m = re_tapeabs.match(l)
    if m is not None:
        p = int(m.group(1))
        cfg = int(m.group(2)) - MINTAPE
        t = [int(x) for x in m.group(3).split()]
        current_l['cfg'][p][cfg]['tape'] = t
        current_r['cfg'][p][cfg]['tape'] = list(reversed(t))
        continue

    m = re_tapemax.match(l)
    if m is not None:
        p = int(m.group(1))
        cfg = int(m.group(2)) - MINTAPE
        t = [x.split('/') for x in m.group(3).split()]
        current_l['cfg'][p][cfg]['tapemax'] = [int(x[0]) for x in t]
        current_r['cfg'][p][cfg]['tapemax'] = list(reversed([int(x[1]) for x in t]))
        continue

# assign numeric ids for progs

proglist = progs.keys()
proglist.sort()

nprogs = len(proglist)
progmap = dict((prog, pid) for pid, prog in enumerate(proglist))

# dump the python structure for direct analysis

pickle.dump({'progs': progs, 'proglist': proglist, 'progmap': progmap, 'proginfo': proginfo},
            open('results.dat', 'wb'))

# generate the (masked) array representation for scores and duel points

scores = np.nan * np.zeros((nprogs, NCFG*nprogs))
cycles = np.nan * np.zeros((nprogs, NCFG*nprogs))

dpoints = np.nan * np.zeros((nprogs, nprogs))
dcycles = np.nan * np.zeros((nprogs, nprogs))

for row, prog in enumerate(proglist):
    for opponent, match in progs[prog].iteritems():
        col = progmap[opponent]

        scores[row, col*NCFG:(col+1)*NCFG] = match['score']
        cycles[row, col*NCFG:(col+1)*NCFG] = [c['cycles'] for c in chain(match['cfg'][0], match['cfg'][1])]

        dpoints[row, col] = np.array(match['score']).sum()
        dcycles[row, col] = match['cycles']

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

# dump out the "worth" values to know which people to beat in evo

worder = sorted(range(nprogs), key=lambda i: points[i], reverse=True)

for i in worder:
    print "%s.bfjoust %.3f" % (proglist[i], (points[i]+nprogs)/(2*(nprogs-1)))

# dump results for further processing

np.savez('results.npz',
         points=points,
         tscores=tscores,
         scores=scores.filled(np.nan),
         pscores=pscores,
         dpoints=dpoints.filled(np.nan),
         cycles=cycles.filled(np.nan),
         dcycles=dcycles.filled(np.nan),
         proglist=proglist)
