# Keywords: Python, tree-sequence recording, tree sequence recording

import msprime, pyslim

ts = msprime.sim_ancestry(samples=5000, population_size=5000, sequence_length=1e8,
    recombination_rate=1e-8)
slim_ts = pyslim.annotate(ts, model_type="WF", tick=1)
slim_ts.dump("recipe_17.8.trees")
