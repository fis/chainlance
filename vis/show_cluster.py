#! /usr/bin/env python

import matplotlib
matplotlib.use('Cairo')
import matplotlib.pyplot as plt

import numpy as np
import numpy.ma as ma
import scipy.cluster.hierarchy as clust

r = np.load('results.npz')
proglist = r['proglist']
scores = ma.masked_invalid(r['scores'])
cycles = np.log10(ma.masked_invalid(r['cycles']))

nprogs = scores.shape[0]
ncfg = scores.shape[1] / nprogs

# build the distance matrix based on common opponents

dist = np.nan * np.zeros((nprogs, nprogs))

for i in xrange(nprogs-1):
    for j in xrange(i+1, nprogs):
        #d = abs(scores[i,:] - scores[j,:]).sum()
        #d = abs(cycles[i,:] - cycles[j,:]).sum()
        d = abs(scores[i,:]*cycles[i,:] - scores[j,:]*cycles[j,:]).sum()
        dist[i,j] = d

dist = dist[np.isfinite(dist)]

# cluster things hierarchically

plt.figure(figsize=(10, 8), dpi=100)
plt.subplots_adjust(0, .05, .7, 1)

l = clust.linkage(dist, method='average')
clust.dendrogram(l, orientation='right', count_sort=True, labels=proglist, leaf_rotation=0)

for tick in plt.gca().yaxis.get_major_ticks():
    tick.label1.set_fontsize('xx-small')

plt.draw()

plt.savefig('clust')
