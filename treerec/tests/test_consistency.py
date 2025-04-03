import tskit, msprime, pyslim
import numpy as np
import pytest

from recipe_specs import recipe_eq

def node_has_data(ts, n):
    # some chromosome types are represented by a single haplosome
    # in SLiM but still two nodes in the output, and not (currently)
    # distinguishable in any way except they have only missing data
    return (
        np.sum(ts.edges_parent == n) + np.sum(ts.edges_child == n)
        > 0
    )

@pytest.mark.parametrize('recipe', recipe_eq("mutations"), indirect=True)
class TestWithMutations:

    def test_mutation_consistency(self, recipe):
        for result in recipe["results"]:
            slim, slim_ids = result.mutation_output()
            for tsl in result.get_ts():
                for chrom_id in tsl:
                    ts = tsl[chrom_id]
                    assert ts.num_mutations > 0, ts.metadata['SLiM']['this_chromosome']
                    # this is a dictionary of SLiM -> tskit ID (from metadata in nodes)
                    ids = result.get_slim_ids(ts)
                    for sid in ids:
                        has_data = node_has_data(ts, ids[sid])
                        if ts.node(ids[sid]).is_sample() and has_data:
                            assert sid in slim_ids
                    # this is a dict of tskit ID -> index in samples
                    msp_samples = {}
                    for k, u in enumerate(ts.samples()):
                        msp_samples[u] = k
                    pos = -1
                    for var in ts.variants(isolated_as_missing=False):
                        pos += 1
                        while pos < int(var.position):
                            # invariant sites: no genotypes
                            assert pos < ts.sequence_length
                            assert (chrom_id, pos) not in slim
                            pos += 1
                        key = (chrom_id, pos)
                        # print("-----------------")
                        # print("chrom_id, pos:", key)
                        # print("slim:", None if key not in slim else slim[key])
                        # print(var)
                        for j in ids:
                            if ids[j] in msp_samples:
                                sample_num = msp_samples[ids[j]]
                                geno = var.genotypes[sample_num]
                                msp_genotypes = var.alleles[geno].split(",")
                                if (key not in slim) or (j not in slim[key]):
                                    # no mutations at this site
                                    assert msp_genotypes == [''], f"sid={j}, msp={ids[j]}, {key in slim} / {j in slim[key]}\n   {slim[key]}"
                                else:
                                    assert set(msp_genotypes) == set([str(x) for x in slim[key][j]]), f"slim: {slim[key][j]}"


@pytest.mark.parametrize('recipe', recipe_eq("marked_mutations"), indirect=True)
class TestMarkedMutations:

    def check_consistency(self, x, y):
        # here y is an iterable of disjoint lists of ids (determined by indivs
        # who share mutations in SLiM) and x is a dict whose keys are ids and
        # whose values are labels (the roots in msprime); check that any two
        # ids in the same element of y have the same label in x
        all_ids = []
        for a in y:
            if len(y[a]) > 0:
                a_labels = []
                for u in y[a]:
                    assert u in x
                    assert u not in all_ids
                    all_ids.append(u)
                    a_labels.append(x[u])
                print('IDs sharing mutation', a, ":", y[a])
                print('labels of roots in from tree:', a_labels)
                assert len(set(a_labels)) == 1

    def test_marked_consistency(self, recipe):
        for result in recipe["results"]:
            # load tree sequence representations
            for tsl in result.get_ts():
                for chrom_id in tsl:
                    ts = tsl[chrom_id]
                    assert ts.num_mutations > 0
                    # this is a dictionary of SLiM -> msprime ID (from metadata in nodes)
                    slim = result.marked_mutation_output(ts)[chrom_id]
                    pos = 0
                    for t in ts.trees():
                        # get partition of leaves from this tree, using SLiM IDs
                        fams = {}
                        for x in t.nodes():
                            u = x
                            while t.parent(u) != tskit.NULL:
                                u = t.parent(u)
                            fams[x] = u
                        while pos < t.interval[1]:
                            print("pos:", pos, "------------")
                            self.check_consistency(fams, slim[pos])
                            pos += 1


@pytest.mark.parametrize('recipe', recipe_eq("individuals"), indirect=True)
class TestIndividuals:

    def test_individual_consistency(self, recipe):
        # load tree sequence
        for result in recipe["results"]:
            slim_individuals = result.individual_output()
            for tsl in result.get_ts():
                for chrom_id in tsl:
                    ts = tsl[chrom_id].simplify(filter_individuals=True)
                    # this is a dictionary of SLiM -> tskit ID (from metadata in nodes)
                    ids = result.get_slim_ids(ts)
                    # this is a dict of tskit ID -> index in samples
                    # check that all the individual data lines up
                    ts_individuals = {i.metadata['pedigree_id']: i for i in ts.individuals()}
                    alive = {
                            ts.individual(i).metadata['pedigree_id']
                            for i in pyslim.individuals_alive_at(ts, 0)
                    }
                    num_expected = 0
                    for ped_id, slim_ind in slim_individuals.items():
                        if slim_ind.type.startswith("retain"):
                            # retained individuals should only be present in the TS if they
                            # have genomic material. We should check which have nodes that exist
                            ts_nodes = [ids[n] for n in slim_ind.nodes if n in ids]
                            if len(ts_nodes) > 0:
                                num_expected += 1
                                assert ped_id in ts_individuals
                                metadata = ts_individuals[ped_id].metadata
                                assert slim_ind.population == metadata['subpopulation']
                                assert np.allclose(slim_ind.pos, ts_individuals[ped_id].location)
                                for n in ts_nodes:
                                    assert not ts.node(n).is_sample()
                            else:
                                assert ped_id not in ts_individuals
                        elif slim_ind.type=="remember":
                            # permanently "remember"ed individuals and ones that exist during ts
                            # output are always present, and their nodes should be marked as
                            # as samples
                            num_expected += 1
                            assert ped_id in ts_individuals
                            metadata = ts_individuals[ped_id].metadata
                            assert slim_ind.population == metadata['subpopulation']
                            assert np.allclose(slim_ind.pos, ts_individuals[ped_id].location)
                            ts_nodes = set([ids[n] for n in slim_ind.nodes])
                            ind_nodes = ts_individuals[ped_id].nodes
                            # haplosomes can legit have all missing data in the tree sequence
                            # in certain circumstances; for instance, if they have no coalescent
                            # ancestors (since we simplify above), or if they were in the
                            # original generation and Remembered.
                            has_data = [node_has_data(ts, n) for n in ind_nodes]
                            for n, h in zip(ind_nodes, has_data):
                                assert n in ts_nodes or not h
                            for n in ts_nodes:
                                assert n in ind_nodes
                                assert ts.node(n).is_sample()
                            if ped_id not in alive:
                                assert slim_ind.type == "remember"
                        elif slim_ind.type=="output":
                            num_expected += 1
                            
                    assert num_expected == ts.num_individuals
