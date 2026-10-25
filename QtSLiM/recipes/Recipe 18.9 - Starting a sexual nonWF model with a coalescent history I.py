# Keywords: Python, nonWF, non-Wright-Fisher, tree-sequence recording, tree sequence recording

import msprime, pyslim, random
import numpy as np

ts = msprime.sim_ancestry(samples=5000, population_size=5000,
    sequence_length=1e8, recombination_rate=1e-8)

tables = ts.dump_tables()
pyslim.annotate_tables(tables, model_type="nonWF", tick=1, stage="early")

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
mut_id = pyslim.next_slim_mutation_id(ts)
site_num = tables.sites.add_row(position=5000, ancestral_state='')
tables.mutations.add_row(
        node=mut_node_id,
        site=site_num,
        derived_state=str(mut_id),
        time=mut_node.time,
        metadata={"slim_ids": [mut_id]})

# now, mutation info in top-level metadata
pyslim.add_mutation_metadata_tables(tables)
mut_info = tables.metadata['SLiM_mutation_list']
for md in mut_info:
    if md["mutation_id"] == mut_id:
        md["mutation_type"] = 2
        md["per_trait"][0]["effect_size"] = 0.1 # selection coefficient

tmd = tables.metadata
tmd['SLiM_mutation_list'] = mut_info
tables.metadata = tmd

slim_ts = tables.tree_sequence()
slim_ts.dump("coalsex.trees")
