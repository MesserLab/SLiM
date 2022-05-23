# Keywords: Python, tree-sequence recording, tree sequence recording

import tskit

ts = tskit.load("recipe_17.8_II.trees").simplify()

for tree in ts.trees():
    for root in tree.roots:
        print(tree.time(root))
