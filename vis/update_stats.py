#! /usr/bin/env python

# update the egojoust statistics collection based on results.dat/results.npz

import cPickle as pickle
import subprocess
import time
from itertools import chain

import matplotlib
matplotlib.use('Cairo')
import matplotlib.pyplot as plt

import numpy as np
import numpy.ma as ma
import scipy.cluster.hierarchy as clust
from scipy.interpolate import interp1d

ONLYPLOT = None # use None to draw all, 'foo' to update only plot foo

# import the data

t = pickle.load(open('results.dat'))
progs = t['progs']
progmap = t['progmap']
proginfo = t['proginfo']

t = np.load('results.npz')
tscores = t['tscores']
scores = t['scores']
points = t['points']
cycles = t['cycles']
dpoints = t['dpoints']
dcycles = t['dcycles']
proglist = t['proglist']

scores = ma.masked_invalid(scores)
cycles = ma.masked_invalid(cycles)
dpoints = ma.masked_invalid(dpoints)
dcycles = ma.masked_invalid(dcycles)

nprogs = len(proglist)

MINTAPE = 10
MAXTAPE = 30
NTAPES = MAXTAPE - MINTAPE + 1

# generate raw-id/score-based-position mapping

ordr = sorted(xrange(nprogs), key=lambda i: tscores[i], reverse=True)

iordr = range(nprogs)
for i in xrange(nprogs):
    iordr[ordr[i]] = i

# prepare our custom colormap

cmapdata = {'red':   [(0.0, 0.0, 0.0), (0.5, 0.4, 0.4), (1.0, 1.0, 1.0)],
            'green': [(0.0, 0.0, 0.0), (0.5, 0.4, 0.4), (1.0, 0.0, 0.0)],
            'blue':  [(0.0, 1.0, 1.0), (0.5, 0.4, 0.4), (1.0, 0.0, 0.0)]}
cmap = matplotlib.colors.LinearSegmentedColormap('redblue', cmapdata)

# open the page

updtime = time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime())

index = open('stats/index.html', 'wt')

# plotting code for various different tournament-wide plots

def plot_dpoints():
    dp = dpoints[np.ix_(ordr[::-1], ordr)]

    plt.subplots_adjust(.3, .3, 1, 1)
    plt.pcolor(dp, cmap=cmap)

    plt.xticks(np.arange(len(proglist))+0.5, proglist[ordr], size='small', rotation=-90)
    plt.yticks(np.arange(len(proglist))+0.5, proglist[ordr[::-1]], size='small')

    plt.gca().set_xticks(np.arange(1, len(proglist)), minor=True)
    plt.gca().set_yticks(np.arange(1, len(proglist)), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

    plt.xlim(0, len(proglist));
    plt.ylim(0, len(proglist));

def plot_tlpoints():
    tlpoints = np.zeros((nprogs, NTAPES))
    for tl in xrange(NTAPES):
        tlpoints[:,tl] = scores[:,tl::NTAPES].sum(1)
    tlpoints /= 2

    plt.subplots_adjust(.3, .05, .95, .95)
    plt.pcolor(tlpoints[ordr[::-1],:], cmap=cmap)

    plt.xticks(np.arange(NTAPES)+0.5, [repr(x) for x in xrange(MINTAPE, MAXTAPE+1)])
    plt.yticks(np.arange(nprogs)+0.5, proglist[ordr[::-1]])

    plt.gca().set_xticks(np.arange(1, NTAPES), minor=True)
    plt.gca().set_yticks(np.arange(1, nprogs), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

    plt.xlim(0, NTAPES)
    plt.ylim(0, nprogs)
    plt.clim(-(nprogs-1), nprogs-1)
    plt.colorbar()

def plot_cluster():
    dist = np.nan * np.zeros((nprogs, nprogs))
    for i in xrange(nprogs-1):
        for j in xrange(i+1, nprogs):
            d = abs(scores[i,:] - scores[j,:]).sum()
            dist[i,j] = d
    dist = dist[np.isfinite(dist)]

    plt.subplots_adjust(0, .05, .7, 1)

    l = clust.linkage(dist, method='average')
    clust.dendrogram(l, orientation='right', count_sort=True, labels=proglist, leaf_rotation=0)

def plot_lenscore():
    lens = np.zeros((nprogs, 1))
    for prog, nfo in proginfo.iteritems():
        lens[progmap[prog]] = nfo['len']

    plt.semilogx(lens, tscores, 'bo')
    plt.xlabel('Program size in ops')
    plt.ylabel('Tournament score')

def plot_cycles():
    dc = dcycles[np.ix_(ordr[::-1], ordr)]

    plt.gcf().set_size_inches(11.5, 10)
    plt.subplots_adjust(.3, .3, 1, 1)
    plt.pcolor(np.log10(dc), cmap=cmap)
    plt.colorbar()

    plt.xticks(np.arange(len(proglist))+0.5, proglist[ordr], size='small', rotation=-90)
    plt.yticks(np.arange(len(proglist))+0.5, proglist[ordr[::-1]], size='small')

    plt.gca().set_xticks(np.arange(1, len(proglist)), minor=True)
    plt.gca().set_yticks(np.arange(1, len(proglist)), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

    plt.xlim(0, len(proglist));
    plt.ylim(0, len(proglist));

def plot_cycleh():
    plt.gcf().set_size_inches(10, 6)
    plt.subplots_adjust(.1, .05, .95, .95)
    plt.hist(np.log10(cycles.compressed()), bins=100)
    plt.xlim(0, 5)
    plt.xlabel('Match length in 10^x cycles')
    plt.ylabel('Number of jousts in the bin')

def plot_cyclewl():
    wcycles = cycles.copy()
    lcycles = cycles.copy()
    wcycles.mask[scores < 0] = True
    lcycles.mask[scores > 0] = True
    wcycles = wcycles.mean(1).filled(0)
    lcycles = lcycles.mean(1).filled(0)
    wlcycles = np.concatenate((wcycles.reshape((1,nprogs)), lcycles.reshape((1,nprogs))), 0)

    plt.gcf().set_size_inches(6, 12)
    plt.subplots_adjust(.05, .05, .5, .95)
    plt.plot([0, 1], wlcycles)

def plot_tapeabs():
    N = 256 # number of cells for interpolation

    tapes = np.zeros((nprogs, N))
    tapecount = np.zeros((nprogs, 1))

    for i, pn in enumerate(proglist):
        for match in progs[pn].itervalues():
            for cfg in chain(match['cfg'][0], match['cfg'][1]):
                tabs = abs(np.array(cfg['tape']))
                tf = interp1d(xrange(len(tabs)), tabs)
                tapes[i, :] += tf(np.linspace(0, len(tabs)-1, N))
                tapecount[i] += 1

    tapes /= tapecount

    plt.subplots_adjust(.08, .08, .92, .92)
    plt.plot(np.linspace(0, 1, N), tapes[ordr[:7],:].T)

    plt.xticks([0, 1], ['home', 'opp. flag'])
    plt.ylabel('absolute value left on tape')
    plt.legend([proglist[i] for i in ordr[:7]])

def plot_tapeheat():
    N = 256 # number of cells for interpolation

    tapes = np.zeros((nprogs, N))
    tapecount = np.zeros((nprogs, 1))

    for i, pn in enumerate(proglist):
        for match in progs[pn].itervalues():
            for cfg in chain(match['cfg'][0], match['cfg'][1]):
                tabs = abs(np.array(cfg['tapeheat']))
                tf = interp1d(xrange(len(tabs)), tabs)
                tapes[i, :] += tf(np.linspace(0, len(tabs)-1, N))
                tapecount[i] += 1

    tapes /= tapecount

    plt.subplots_adjust(.08, .08, .92, .92)
    plt.semilogy(np.linspace(0, 1, N), tapes[ordr[:7],:].T)

    plt.xticks([0, 1], ['home', 'opp. flag'])
    plt.ylabel('average number of cycles')
    plt.legend([proglist[i] for i in ordr[:7]], loc='upper center')

# single-program plots

def plot_ptapeabs(pn, p):
    tapes = np.zeros((NTAPES, MAXTAPE))
    tapecount = np.zeros((NTAPES, 1))

    for match in p.itervalues():
        for cfg in chain(match['cfg'][0], match['cfg'][1]):
            tabs = abs(np.array(cfg['tape']))
            tapes[len(tabs)-MINTAPE, 0:len(tabs)] += tabs
            tapecount[len(tabs)-MINTAPE] += 1

    tapes /= tapecount

    for tlen in xrange(MINTAPE, MAXTAPE):
        tapes[tlen-MINTAPE, tlen:] = np.nan
    tapes = ma.masked_invalid(tapes)

    plt.subplots_adjust(.08, .08, .98, .92)
    plt.pcolor(tapes, cmap=cmap)
    plt.colorbar()

    plt.xticks(np.array([1]+range(MINTAPE, MAXTAPE+1, 10))-0.5, ['Home']+[repr(x) for x in xrange(MINTAPE, MAXTAPE+1, 10)])
    plt.xlim(0, MAXTAPE)
    plt.yticks(np.array(xrange(NTAPES))+.5, [repr(x) for x in xrange(MINTAPE,MAXTAPE+1)])
    plt.ylim(0, NTAPES)
    plt.ylabel('Tape length')

    plt.gca().set_yticks(xrange(NTAPES+1), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

def plot_ptapemax(pn, p):
    tapes = np.zeros((NTAPES, MAXTAPE))
    tapecount = np.zeros((NTAPES, 1))

    for match in p.itervalues():
        for cfg in chain(match['cfg'][0], match['cfg'][1]):
            tabs = np.array(cfg['tapemax'])
            tapes[len(tabs)-MINTAPE, 0:len(tabs)] += tabs
            tapecount[len(tabs)-MINTAPE] += 1

    tapes /= tapecount

    for tlen in xrange(MINTAPE, MAXTAPE):
        tapes[tlen-MINTAPE, tlen:] = np.nan
    tapes = ma.masked_invalid(tapes)

    plt.subplots_adjust(.08, .08, .98, .92)
    plt.pcolor(tapes, cmap=cmap)
    plt.colorbar()

    plt.xticks(np.array([1]+range(MINTAPE, MAXTAPE+1, 10))-0.5, ['Home']+[repr(x) for x in xrange(MINTAPE, MAXTAPE+1, 10)])
    plt.xlim(0, MAXTAPE)
    plt.yticks(np.array(xrange(NTAPES))+.5, [repr(x) for x in xrange(MINTAPE,MAXTAPE+1)])
    plt.ylim(0, NTAPES)
    plt.ylabel('Tape length')

    plt.gca().set_yticks(xrange(NTAPES+1), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

def plot_ptapeheat(pn, p):
    tapes = np.zeros((NTAPES, MAXTAPE))
    tapecount = np.zeros((NTAPES, 1))

    for match in p.itervalues():
        for cfg in chain(match['cfg'][0], match['cfg'][1]):
            tabs = np.array(cfg['tapeheat'])
            tapes[len(tabs)-MINTAPE, 0:len(tabs)] += tabs
            tapecount[len(tabs)-MINTAPE] += 1

    tapes /= tapecount

    tapes[tapes == 0] = 0.001
    tapes = np.log10(tapes)

    for tlen in xrange(MINTAPE, MAXTAPE):
        tapes[tlen-MINTAPE, tlen:] = np.nan
    tapes = ma.masked_invalid(tapes)

    plt.subplots_adjust(.08, .08, .98, .92)
    plt.pcolor(tapes, cmap=cmap)
    plt.colorbar()

    plt.xticks(np.array([1]+range(MINTAPE, MAXTAPE+1, 10))-0.5, ['Home']+[repr(x) for x in xrange(MINTAPE, MAXTAPE+1, 10)])
    plt.xlim(0, MAXTAPE)
    plt.yticks(np.array(xrange(NTAPES))+.5, [repr(x) for x in xrange(MINTAPE,MAXTAPE+1)])
    plt.ylim(0, NTAPES)
    plt.ylabel('Tape length')

    plt.gca().set_yticks(xrange(NTAPES+1), minor=True)
    plt.grid(True, which='minor', linestyle='-', color='k')

# plotter wrapper for saving figures with thumbnails

def savefig(fname, fnametn):
    plt.savefig('stats/'+fname)
    subprocess.check_call(["convert", 'stats/'+fname, '-resize', '100x100', 'stats/'+fnametn])

def plot(name, plotfn, progidx):
    fbase = name if progidx is None else 'p%d_%s' % (progidx, name)
    fname = 'plot_%s.png' % fbase
    fnametn = 'tn_plot_%s.png' % fbase
    if ONLYPLOT is not None and ONLYPLOT != name: return fname, fnametn
    plt.figure(figsize=(10, 10), dpi=100)
    if progidx is None: plotfn()
    else: plotfn(proglist[progidx], progs[proglist[progidx]])
    savefig(fname, fnametn)
    return fname, fnametn

def linkplot(name, plotfn, desc, progidx=None):
    fname, fnametn = plot(name, plotfn, progidx)
    index.write('      <a class="plot" href="%s"><img class="plot" src="%s" /></a>\n' %
                (fname, fnametn))
    index.write('      <p class="plotdesc">%s</p>\n' % desc.replace('\n', ' ').strip())

# generate the web page with tables and links

index.write('<!DOCTYPE html>\n')
index.write('<html>\n')
index.write('  <head>\n')
index.write('    <title>egojoust statistics -- %s</title>\n' % updtime)
index.write('    <link rel="stylesheet" href="stats.css" />\n')
index.write('    <script type="text/javascript" src="stats.js" charset="UTF-8"></script>\n')
index.write('  </head>\n')
index.write('  <body>\n')

# main summary table

index.write('    <h1>Tournament-wide statistics</h1>\n')
index.write('    <div id="tg1" class="tg">\n')
index.write('    <h2 id="summary" class="sect">Summary table</h2>\n')

index.write('    <table id="ssummary" class="sect">\n')
index.write('      <tr><th>Pos</th><th>Score</th><th>Points</th><th>Program</th></tr>\n')
for pos, i in enumerate(ordr):
    index.write('      <tr><td class="summarypos">%d</td><td>%.2f</td><td>%.2f</td><td>%s</td></tr>\n' %
                (pos+1, tscores[i], points[i], proglist[i]))
index.write('    </table>\n')

# touranment-wide plots

print "generating tournament-wide plots"

index.write('    <h2 id="tplots" class="sect">Plots</h2>\n')
index.write('    <div id="stplots" class="sect">\n')

linkplot('dpoints', plot_dpoints, '''
Per-duel scores (sum of per-configuration scores, with win +1, loss
-1).  Solid red indicates a total victory for the program on the
particular row; neutral grey indicates a tie; solid blue is a loss.
''')

index.write('      <hr class="midplot" />\n')

linkplot('tlpoints', plot_tlpoints, '''
Average duel points for each program for the different tape lengths.
''')

index.write('      <hr class="midplot" />\n')

linkplot('cluster', plot_cluster, '''
Hierarchical clustering of programs based on the duel scores.  The
distance measure used here is: for any two programs A and B, take the
scores (as 2*21-dimensional vectors of -1, 0, 1) against all N-2
common opponents, concatenate, and take the Manhattan (1-norm)
distance.  For two clusters, the distance measure used is average of
pairwise distances.  The figure shows a dendrogram with colors
assigned to clusters just below the 0.7*maximum distance.
''')

index.write('      <hr class="midplot" />\n')

linkplot('lenscore', plot_lenscore, '''
Scatterplot of program length against achieved tournament score.
''')

index.write('      <hr class="midplot" />\n')

linkplot('cycles', plot_cycles, '''
Duel lengths in cycles.  The color bar gives lengths in units of
10<sup>x</sup>; i.e. the scale is logarithmic.  Theoretical maximum is
4200000 cycles: 2*21 jousts each at 100k cycles.
''')

index.write('      <hr class="midplot" />\n')

linkplot('cycleh', plot_cycleh, '''
Overall duel length histogram.  This is again logarithmic on the X
axis, but I can't coax a proper logscale histogram out of matplotlib,
so the X axis labels are again 10<sup>x</sup> values.  Usually there
will be a peak at 10<sup>5</sup> = 100000.
''')

index.write('      <hr class="midplot" />\n')

linkplot('tapeabs', plot_tapeabs, '''
Absolute values (magnitude of differences from 0) left on the tape at
the end of the match, as seen from the viewpoint of the 7 best
programs, averaged over all opponents, tape lengths and polarities.
See per-program statistics for single-program per-tape-length tables.
''')

index.write('      <hr class="midplot" />\n')

linkplot('tapeheat', plot_tapeheat, '''
Number of cycles spent in a particular area of the tape, as seen from
the viewpoint of the 7 best programs, averaged over all matches.
''')

index.write('    </div>\n')

# random tournament-wide statistics

index.write('    <h2 id="miscstat" class="sect">Miscellaneous statistics</h2>\n')

index.write('    <div id="smiscstat" class="sect">\n')
index.write('      <p>Simulated a total of %d cycles in %d individual jousts for %d duels.</p>\n' %
            (dcycles.sum()/2, (~scores.mask).sum()/2, (~dpoints.mask).sum()/2))
index.write('      <p>Statistics page generated at %s UTC.</p>' % updtime)
index.write('    </div>\n')

index.write('    </div>\n')

# per-program statistics

index.write('    <h1>Per-program statistics</h1>\n')
index.write('    <div id="tg2" class="tg">\n')

for i, prog in enumerate(proglist):
    print "generating per-program plots for prog %d/%d: %s" % (i+1, len(proglist), prog)

    index.write('    <h2 id="prog%d" class="sect">%s</h2>\n' % (i, prog))
    index.write('    <div id="sprog%d" class="sect">\n' % i)

    linkplot('ptapeabs', plot_ptapeabs, '''
Absolute values (magnitude of differences from 0) left on the tape at
the end of a match, as seen from the viewpoint of this program, as a
function of tape length.
''', i)

    index.write('      <hr class="midplot" />\n')

    linkplot('ptapemax', plot_ptapemax, '''
Averages over maximum tape cell values "left" on the tape as the
program executed a &lt; or a &gt; to move away from the cell.  In some
cases this corresponds to set decoys and such; but do note that this
will copy whatever structures the opponent program has made on the
tape, if this program passes over it.
''', i)

    index.write('      <hr class="midplot" />\n')

    linkplot('ptapeheat', plot_ptapeheat, '''
Number of cycles the program's tape head has spent in a particular cell,
averaged over all duels with the same tape length.  The color bar scale
is logarithmic: value of x denotes 10<sup>x</sup> cycles.
''', i)

    index.write('    </div>\n')

index.write('    </div>\n')

# close the page

index.write("  </body>\n")
index.write("</html>\n")
index.close()
