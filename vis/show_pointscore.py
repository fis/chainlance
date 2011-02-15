#! /usr/bin/env python

import numpy as np
import numpy.ma as ma

import matplotlib
matplotlib.use('Cairo')
import matplotlib.pyplot as plt

r = np.load('results.npz')
tscores = r['tscores']
points = r['points']
proglist = r['proglist']

plt.figure(figsize=(10, 10), dpi=100)
plt.subplots_adjust(.05, .05, .95, .95)

plt.scatter(points, tscores)

plt.savefig('pointscore')
