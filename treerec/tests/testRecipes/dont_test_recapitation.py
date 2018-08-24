import msprime
import pyslim
import numpy as np

def unmark_first_generation(ts):
    tables = ts.dump_tables()
    flags = tables.nodes.flags
    flags[tables.nodes.time == ts.slim_generation] = 0
    tables.nodes.set_columns(flags=flags, population=tables.nodes.population,
            individual=tables.nodes.individual, time=tables.nodes.time,
            metadata=tables.nodes.metadata, 
            metadata_offset=tables.nodes.metadata_offset)
    return pyslim.load_tables(tables, slim_format=True)

decap = pyslim.load("temp/decapitated.trees", slim_format=True)

unmarked_recap = []
recap_unmarked = []
recap_unmarked_simplified = []
for _ in range(10):
    recap = decap.recapitate(recombination_rate=1e-6, Ne=1000)
    recap_mut = pyslim.mutate(recap,
                rate=1e-6, keep=True)
    ru = unmark_first_generation(recap_mut)
    recap_unmarked.append(ru)
    ru = ru.simplify(ru.samples())
    recap_unmarked_simplified.append(ru)
    recap_unmarked.append(ru)
    decap_unmarked = unmark_first_generation(decap)
    unmarked_recap.append(pyslim.mutate(
                decap_unmarked.recapitate(recombination_rate=1e-6, Ne=1000),
                rate=1e-6, keep=True))


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

print("recapitated | unmarked:")
recap_unmarked_stats = summarize_stats(recap_unmarked)
print("-----------------------")
print("recapitated | unmarked | simplified:")
recap_unmarked_simplified_stats = summarize_stats(recap_unmarked_simplified)
print("-----------------------")
print("unmarked | recapitated:")
unmarked_recap_stats = summarize_stats(unmarked_recap)

print("-----------------------")
print("  Summaries: ")

print("decapitated:")
get_stats(decap)
print("\n")

print("recapitated | unmarked:")
print(recap_unmarked_stats)
print("\n")

print("recapitated | unmarked | simplified:")
print(recap_unmarked_stats)
print("\n")

print("unmarked | recapitated:")
print(unmarked_recap_stats)
print("\n")
