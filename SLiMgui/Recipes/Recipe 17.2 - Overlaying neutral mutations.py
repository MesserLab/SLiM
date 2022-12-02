# Keywords: Python, tree-sequence recording, tree sequence recording

# This is a Python recipe, to be run after the section 17.1 recipe

import msprime, tskit

ts = tskit.load("./overlay.trees")
ts = ts.simplify()

for t in ts.trees():
    assert t.num_roots == 1, ("not coalesced! on segment {} to {}".format(t.interval[0], t.interval[1]))

mutated = msprime.sim_mutations(ts, rate=1e-7, random_seed=1, keep=True)
mutated.dump("./overlay_II.trees")
