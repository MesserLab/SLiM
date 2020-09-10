# Keywords: Python, tree-sequence recording, tree sequence recording

import msprime, pyslim

ts = pyslim.load("recipe_17.7.trees")

# selection coefficients of all selected mutations
coeffs = []
for mut in ts.mutations():
    md = mut.metadata
    sel = [x["selection_coeff"] for x in md["mutation_list"]]
    if any([s != 0 for s in sel]):
        coeffs += sel

b = [x for x in coeffs if x > 0]
d = [x for x in coeffs if x < 0]

print("Beneficial: " + str(len(b)) + ", mean " + str(sum(b) / len(b)))
print("Deleterious: " + str(len(d)) + ", mean " + str(sum(d) / len(d)))
