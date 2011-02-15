#! /usr/bin/env python

import numpy as np
import numpy.ma as ma

import matplotlib
matplotlib.use('Cairo')
import matplotlib.pyplot as plt

r = np.load('results.npz')
dcycles = r['dcycles']
tscores = r['tscores']
points = r['points']
proglist = r['proglist']

#order = np.argsort(tscores)
order = np.argsort(points)

dcycles = dcycles[np.ix_(order, order[::-1])]
dcycles = ma.masked_invalid(dcycles)

# duel matrix form

plt.figure(figsize=(11.5, 10), dpi=100)
plt.subplots_adjust(.3, .3, 1, 1)

cmd = {'red':   [(0.0, 0.0, 0.0), (0.5, 0.4, 0.4), (1.0, 1.0, 1.0)],
       'green': [(0.0, 0.0, 0.0), (0.5, 0.4, 0.4), (1.0, 0.0, 0.0)],
       'blue':  [(0.0, 1.0, 1.0), (0.5, 0.4, 0.4), (1.0, 0.0, 0.0)]}
cm = matplotlib.colors.LinearSegmentedColormap('redblue', cmd)

plt.pcolor(np.log10(dcycles), cmap=cm)
plt.colorbar()

plt.xticks(np.arange(len(proglist))+0.5, proglist[order[::-1]], size='small', rotation=-90)
plt.yticks(np.arange(len(proglist))+0.5, proglist[order], size='small')

plt.gca().set_xticks(np.arange(1, len(proglist)), minor=True)
plt.gca().set_yticks(np.arange(1, len(proglist)), minor=True)
plt.grid(True, which='minor', linestyle='-', color='k')

plt.xlim(0, len(proglist));
plt.ylim(0, len(proglist));

plt.savefig('cycles')
plt.close('all')

# tournament length histogram

cycles = r['cycles']
cycles = ma.masked_invalid(cycles)

plt.figure(figsize=(10, 10), dpi=100)
plt.subplots_adjust(.05, .05, .95, .95)

plt.hist(np.log10(cycles.compressed()), bins=100)

plt.savefig('cycleh')
