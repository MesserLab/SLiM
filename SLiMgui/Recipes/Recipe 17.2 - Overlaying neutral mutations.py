# Keywords: Python, tree-sequence recording, tree sequence recording

# This is a Python recipe, to be run after the section 16.1 recipe

import msprime, pyslim

ts = pyslim.load("./recipe_16.1.trees").simplify()
mutated = msprime.mutate(ts, rate=1e-7, random_seed=1, keep=True)
mutated.dump("./recipe_16.1_overlaid.trees")
