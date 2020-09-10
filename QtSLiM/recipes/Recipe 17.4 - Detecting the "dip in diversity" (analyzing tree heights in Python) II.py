# Keywords: Python, tree-sequence recording, tree sequence recording

# This is a Python recipe; note that it runs the SLiM model internally, below

import subprocess, msprime, pyslim
import matplotlib.pyplot as plt
import numpy as np

# Run the SLiM model and load the resulting .trees
subprocess.check_output(["slim", "-m", "-s", "0", "./recipe_17.4.slim"])
ts = pyslim.load("./recipe_17.4.trees")
ts = ts.simplify()

# Measure the tree height at each base position
height_for_pos = np.zeros(int(ts.sequence_length))
for tree in ts.trees():
    mean_height = np.mean([tree.time(root) for root in tree.roots])
    left, right = map(int, tree.interval)
    height_for_pos[left: right] = mean_height

# Convert heights along chromosome into heights at distances from gene
height_for_pos = height_for_pos - np.min(height_for_pos)
L, L0, L1 = int(1e8), int(200e3), int(1e3)   # total length, length between genes, gene length
gene_starts = np.arange(L0, L - (L0 + L1) + 1, L0 + L1)
gene_ends = gene_starts + L1 - 1
max_d = L0 // 4
height_for_left_dist = np.zeros(max_d)
height_for_right_dist = np.zeros(max_d)
for d in range(max_d):
    height_for_left_dist[d] = np.mean(height_for_pos[gene_starts - d - 1])
    height_for_right_dist[d] = np.mean(height_for_pos[gene_ends + d + 1])
height_for_distance = np.hstack([height_for_left_dist[::-1], height_for_right_dist])
distances = np.hstack([np.arange(-max_d, 0), np.arange(1, max_d + 1)])

# Make a simple plot
plt.plot(distances, height_for_distance)
plt.show()
