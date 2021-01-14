import msprime
import pyslim
import numpy as np

decap = pyslim.load("temp/decapitated.trees", slim_format=True)

recap = []
recap_simplified = []
for _ in range(10):
    recap = decap.recapitate(recombination_rate=1e-6, Ne=1000)
    recap_mut = pyslim.mutate(recap,
                rate=1e-6, keep=True)
    ru = recap.simplify(ru.samples())
    recap_simplified.append(ru)

def get_stats(ts):
    bs = msprime.BranchLengthStatCalculator(ts)
    stats = {'num_trees' : ts.num_trees,
             'sequence_length' : ts.sequence_length,
             'num_samples' : ts.num_samples,
             'num_mutations' : ts.num_mutations,
             'divergence' : -1 # bs.divergence(sample_sets=[list(ts.samples())], windows=[0.0, ts.sequence_length])
             }
    print(stats)
    return stats

def summarize_stats(tslist):
    statlist = list(map(get_stats, tslist))
    out = {}
    for a in statlist[0]:
        x = [u[a] for u in statlist]
        out[a] = {'mean':np.mean(x), 'sd':np.std(x)*np.sqrt(len(x)/(len(x)-1))}
    return out

print("recapitated:")
recap_stats = summarize_stats(recap)
print("-----------------------")
print("recapitated | simplified:")
recap_simplified_stats = summarize_stats(recap_simplified)
print("-----------------------")

print("-----------------------")
print("  Summaries: ")

print("decapitated:")
get_stats(decap)
print("\n")

print("recapitated:")
print(recap_stats)
print("\n")

print("recapitated | simplified:")
print(recap_simplified_stats)
print("\n")
