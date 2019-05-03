# Keywords: Python, tree-sequence recording, tree sequence recording

import msprime, pyslim
import numpy as np
import matplotlib.pyplot as plt

# Load the .trees file
ts = pyslim.load("recipe_17.10_decap.trees")    # no simplify!

# Calculate tree heights, giving uncoalesced sites the maximum time
def tree_heights(ts):
    heights = np.zeros(ts.num_trees + 1)
    for tree in ts.trees():
        if tree.num_roots > 1:  # not fully coalesced
            heights[tree.index] = ts.slim_generation
        else:
            children = tree.children(tree.root)
            real_root = tree.root if len(children) > 1 else children[0]
            heights[tree.index] = tree.time(real_root)
    heights[-1] = heights[-2]  # repeat the last entry for plotting with step
    return heights

# Plot tree heights before recapitation
breakpoints = list(ts.breakpoints())
heights = tree_heights(ts)
plt.step(breakpoints, heights, where='post')
plt.show()

# Recapitate!
recap = ts.recapitate(recombination_rate=3e-10, Ne=1e5, random_seed=1)
recap.dump("recipe_17.10_recap.trees")

# Plot the tree heights after recapitation
breakpoints = list(recap.breakpoints())
heights = tree_heights(recap)
plt.step(breakpoints, heights, where='post')
plt.show()

