# Keywords: Python, nonWF, non-Wright-Fisher, tree-sequence recording, tree sequence recording

import msprime, pyslim, random

ts = msprime.simulate(sample_size=10000, Ne=5000, length=1e8,
    mutation_rate=0.0, recombination_rate=1e-8)

tables = ts.dump_tables()
pyslim.annotate_defaults_tables(tables, model_type="nonWF", slim_generation=1)
individual_metadata = list(pyslim.extract_individual_metadata(tables))
for j in range(len(individual_metadata)):
    individual_metadata[j].sex = random.choice([pyslim.INDIVIDUAL_TYPE_FEMALE, pyslim.INDIVIDUAL_TYPE_MALE])
    individual_metadata[j].age = random.choice([0, 1, 2, 3, 4])

pyslim.annotate_individual_metadata(tables, individual_metadata)
slim_ts = pyslim.load_tables(tables)
slim_ts.dump("recipe_16.9.trees")
