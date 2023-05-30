# Keywords: Python, nonWF, non-Wright-Fisher, tree-sequence recording, tree sequence recording

import msprime, pyslim, random
import numpy as np

ts = msprime.sim_ancestry(samples=5000, population_size=5000,
    sequence_length=1e8, recombination_rate=1e-8)

tables = ts.dump_tables()
pyslim.annotate_tables(tables, model_type="nonWF", tick=1)

# add sexes and ages
individual_metadata = [ind.metadata for ind in tables.individuals]
for md in individual_metadata:
    md["sex"] = random.choice([pyslim.INDIVIDUAL_TYPE_FEMALE, pyslim.INDIVIDUAL_TYPE_MALE])
    md["age"] = random.choice([0, 1, 2, 3, 4])

ims = tables.individuals.metadata_schema
tables.individuals.packset_metadata(
        [ims.validate_and_encode_row(md) for md in individual_metadata])

# add selected mutation
mut_ind_id = random.choice(range(tables.individuals.num_rows))
mut_node_id = random.choice(np.where(tables.nodes.individual == mut_ind_id)[0])
mut_node = tables.nodes[mut_node_id]
mut_metadata = {
        "mutation_list": [
            {
              "mutation_type": 2,
              "selection_coeff": 0.1,
              "subpopulation": mut_node.population,
              "slim_time": int(tables.metadata['SLiM']['tick'] - mut_node.time),
              "nucleotide": -1
            }
        ]
    }
site_num = tables.sites.add_row(position=5000, ancestral_state='')
tables.mutations.add_row(
        node=mut_node_id,
        site=site_num,
        derived_state='1',
        time=mut_node.time,
        metadata=mut_metadata)

slim_ts = tables.tree_sequence()
slim_ts.dump("coalsex.trees")
