# Keywords: Python, tree-sequence recording, tree sequence recording

import tskit

ts = tskit.load("selcoeff.trees")
mutlist = ts.metadata["SLiM_mutation_list"]

# selection coefficients of all selected mutations
coeffs = []
for mut in mutlist:
    sel = mut["per_trait"][0]["effect_size"]
    if sel != 0:
        coeffs.append(sel)

b = [x for x in coeffs if x > 0]
d = [x for x in coeffs if x < 0]

print("Beneficial: " + str(len(b)) + ", mean " + str(sum(b) / len(b)))
print("Deleterious: " + str(len(d)) + ", mean " + str(sum(d) / len(d)))
