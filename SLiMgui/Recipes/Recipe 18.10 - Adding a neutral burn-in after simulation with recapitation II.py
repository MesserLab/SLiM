# Keywords: Python, tree-sequence recording, tree sequence recording

import tskit, pyslim
import numpy as np
import matplotlib.pyplot as plt

# Load the .trees file
ts = tskit.load("decap.trees")    # no simplify!

# Calculate tree heights
def tree_heights(ts):
    heights = np.zeros(ts.num_trees + 1)
    for tree in ts.trees():
        roots = tree.roots
        if len(roots) == 1:
            children = tree.children(roots[0])
            if len(children) == 1:
                roots = children
        root_heights = [tree.time(r) for r in roots]
        assert len(set(root_heights)) == 1
        heights[tree.index] = root_heights[0]
    heights[-1] = heights[-2]  # repeat the last entry for plotting with step
    return heights

# Plot tree heights before recapitation
breakpoints = list(ts.breakpoints())
heights = tree_heights(ts)
plt.step(breakpoints, heights, where='post')
plt.show()

# Recapitate!
recap = pyslim.recapitate(ts, ancestral_Ne=1e5, recombination_rate=3e-10, random_seed=1)
recap.dump("recap.trees")

# Plot the tree heights after recapitation
breakpoints = list(recap.breakpoints())
heights = tree_heights(recap)
plt.step(breakpoints, heights, where='post')
plt.show()
