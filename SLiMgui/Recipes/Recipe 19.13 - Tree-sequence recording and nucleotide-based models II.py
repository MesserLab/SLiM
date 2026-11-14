# Keywords: Python, nucleotide-based, nucleotide sequence, sequence-based mutation rate

import tskit, pyslim

ts = tskit.load("recipe_nucleotides.trees")
mut_metadata = pyslim.mutation_metadata(ts)

M = [[0 for _ in pyslim.NUCLEOTIDES] for _ in pyslim.NUCLEOTIDES]
for mut in ts.mutations():
    derived_mut_id = mut.metadata["slim_ids"][-1]
    derived_nuc = mut_metadata[derived_mut_id]["nucleotide"]
    if mut.parent == -1:
        acgt = ts.reference_sequence.data[int(ts.site(mut.site).position)]
        parent_nuc = pyslim.NUCLEOTIDES.index(acgt)
    else:
        parent_mut = ts.mutation(mut.parent)
        assert(parent_mut.site == mut.site)
        parent_mut_id = parent_mut.metadata["slim_ids"][-1]
        parent_nuc = mut_metadata[parent_mut_id]["nucleotide"]
    M[parent_nuc][derived_nuc] += 1

print("{}\t{}\t{}".format('ancestral', 'derived', 'count'))
for j, a in enumerate(pyslim.NUCLEOTIDES):
    for k, b in enumerate(pyslim.NUCLEOTIDES):
        print("{}\t{}\t{}".format(a, b, M[j][k]))
