# Keywords: Python, nucleotide-based, nucleotide sequence, sequence-based mutation rate

import pyslim
import numpy as np

ts = pyslim.load("recipe_nucleotides.trees")
slim_gen = ts.metadata["SLiM"]["generation"]

M = np.zeros((4,4,4,4), dtype='int')
for mut in ts.mutations():
    pos = ts.site(mut.site).position 
    # skip mutations at the end of the sequence
    if pos > 0 and pos < ts.sequence_length - 1:
        mut_list = mut.metadata["mutation_list"]
        k = np.argmax([u["slim_time"] for u in mut_list])
        derived_nuc = mut_list[k]["nucleotide"]
        left_nuc = ts.nucleotide_at(mut.node, pos - 1, 
                time = slim_gen - mut_list[k]["slim_time"] - 1.0)
        right_nuc = ts.nucleotide_at(mut.node, pos + 1,
                time = slim_gen - mut_list[k]["slim_time"] - 1.0)
        if mut.parent == -1:
            acgt = ts.reference_sequence.data[int(ts.site(mut.site).position)]
            parent_nuc = pyslim.NUCLEOTIDES.index(acgt)
        else:
            parent_mut = ts.mutation(mut.parent)
            assert(parent_mut.site == mut.site)
            parent_nuc = parent_mut.metadata["mutation_list"][0]["nucleotide"]
        M[left_nuc, parent_nuc, right_nuc, derived_nuc] += 1

print("{}\t{}\t{}".format('ancestral', 'derived', 'count'))
for j0, a0 in enumerate(pyslim.NUCLEOTIDES):
    for j1, a1 in enumerate(pyslim.NUCLEOTIDES):
        for j2, a2 in enumerate(pyslim.NUCLEOTIDES):
            for k, b in enumerate(pyslim.NUCLEOTIDES):
                print("{}{}{}\t{}{}{}\t{}".format(a0, a1, a2, a0, b, a2,
                        M[j0, j1, j2, k]))
