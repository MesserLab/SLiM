# Keywords: Python, tree-sequence recording, tree sequence recording

import msprime, pyslim

ts = msprime.simulate(sample_size=10000, Ne=5000, length=1e8,
    mutation_rate=0.0, recombination_rate=1e-8)
slim_ts = pyslim.annotate_defaults(ts, model_type="WF", slim_generation=1)
slim_ts.dump("recipe_17.8.trees")
