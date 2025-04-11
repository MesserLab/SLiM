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
                                # in metadata, sex is 0 for female, 1 for male, -1 for hermaphrodite
                                if slim_ind.sex == "F":
                                    assert metadata['sex'] == 0
                                elif slim_ind.sex == "M":
                                    assert metadata['sex'] == 1
                                elif slim_ind.sex == "H":
                                    assert metadata['sex'] == -1
                                else:
                                    assert False, "Bad sex field in metadata"
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


@pytest.mark.parametrize('recipe', recipe_eq(), indirect=True)
class TestChromosomes:

    def chrom_details(self, chrom):
        # in metadata:
        # sex is 0 for female, 1 for male, -1 for hermaphrodite
        # chrom is:
        # "A" (autosome), the default
        # "H" (haploid), specifying a haploid autosomal chromosome
        #    that recombines in biparental crosses.
        # Some sex-chromosome types are supported only in sexual models:
        # "X" (X), an X chromosome that is diploid (XX) in females, haploid (X-) in males.
        # "Y" (Y), a Y chromosome that is haploid (Y) in males, absent (-) in females.
        # "Z" (Z), a Z chromosome that is diploid (ZZ) in males, haploid (-Z) in females.
        # "W" (W), a W chromosome that is haploid (W) in females, absent (-) in males.
        # And there are haploid chromosome types that are also supported only in sexual models:
        # "HF" (haploid female-inherited), a haploid autosomal chromosome 
        #   inherited by both sexes from the first (female) parent in biparental crosses.
        # "FL" (female line), specifying a haploid autosomal chromosome 
        #   inherited only by females, from the female parent, 
        #   represented by a null haplosome in males.
        # "HM" (haploid male-inherited), a haploid autosomal chromosome 
        #   inherited by both sexes from the second (male) parent in biparental crosses.
        # "ML" (male line), specifying a haploid autosomal chromosome 
        #   inherited only by males, from the male parent, 
        #   represented by a null haplosome in females.
        # Finally, "H-" and "-Y", are supported for backward compatibility.
        #
        # This returns a dictionary with keys:
        # ploidy: ploidy of chromsome; all inds should have this many
        # (F/M/H): number of null chromosomes expected for this sex
        sexes = ["F", "M", "H"]
        if chrom == "A":
            out = { "ploidy" : 2 }
            out.update({ x: 0 for x in sexes })
        elif chrom == "H":
            out = { "ploidy" : 1 }
            out.update({ x: 0 for x in sexes })
        elif chrom == "X":
            out = { "ploidy" : 2, "F": 0, "M": 1 }
        elif chrom == "Y":
            out = { "ploidy" : 1, "F": 1, "M": 0 }
        elif chrom == "Z":
            out = { "ploidy" : 2, "F": 1, "M": 0 }
        elif chrom == "W":
            out = { "ploidy" : 1, "F": 0, "M": 1 }
        elif chrom == "HF":
            out = { "ploidy" : 1, "F": 0, "M": 0 }
        elif chrom == "FL":
            out = { "ploidy" : 1, "F": 0, "M": 1 }
        elif chrom == "HM":
            out = { "ploidy" : 1, "F": 0, "M": 0 }
        elif chrom == "ML":
            out = { "ploidy" : 1, "F": 1, "M": 0 }
        elif chrom == "H-":
            out = { "ploidy" : 2, "F": 1, "M": 1 }
        elif chrom == "-Y":
            out = { "ploidy" : 2, "F": 2, "M": 1 }
        else:
            assert False, f"Unknown chromosome type {chrom}"
        return out

    def is_chrom_null(self, k, b):
        # From the SLiM manual on is_null:
        # M bytes (uint8_t): a series of bytes comprising a bitfield of is_null
        # values, true (1) if this node represents a null haplosome for a given
        # chromosome, false (0) otherwise. For chromosomes with indices 0...N−1, the
        # chromosome with index k has its is_null bit in bit k%8 of byte k/8, where
        # byte 0 is the first byte in the series of bytes provided, and bit 0 is the
        # least-significant bit, the one with value 0x01 (hexadecimal 1). The number
        # of bytes present, M, is equal to (N+7)/8, the minimum number of bytes
        # necessary. The operators / and % here are integer divide (rounding down)
        # and integer modulo, respectively.
        assert len(b) >= (1 + k) / 8
        b = b[int(k / 8)]
        i = k % 8
        return b >> i & 1

    def test_chromosome_consistency(self, recipe):
        # check whether chromsomes are null when they're supposed to be
        # and individuals have the right number of chromosomes
        for result in recipe["results"]:
            for tsl in result.get_ts():
                for chrom_id in tsl:
                    ts = tsl[chrom_id]
                    chrom_type = ts.metadata['SLiM']['this_chromosome']['type']
                    chrom_index = ts.metadata['SLiM']['this_chromosome']['index']
                    details = self.chrom_details(chrom_type)
                    if chrom_type in ['X', 'Y', 'Z', 'W', 'HF', 'FL', 'HM', 'ML']:
                        assert ts.metadata['SLiM']['separate_sexes']
                    for ind in ts.individuals():
                        if ind.flags & (pyslim.INDIVIDUAL_ALIVE | pyslim.INDIVIDUAL_REMEMBERED) > 0:
                            sex = {0 : "F", 1 : "M", -1 : "H"}[ind.metadata['sex']]
                            assert sex in details, f"Invalid sex {sex} for {chrom_type} chrom"
                            assert len(ind.nodes) == 2
                            print(sex, chrom_type, details)
                            print(ind)
                            for n in ind.nodes:
                                print(ts.node(n))
                            # TODO: problems distinguishing null and just missing
                            num_null = 0
                            num_nonmissing = 0
                            for ni in ind.nodes:
                                n = ts.node(ni)
                                has_data = (np.sum(ts.edges_parent == ni)
                                            + np.sum(ts.edges_child == ni) > 0)
                                num_nonmissing += has_data
                                is_null = self.is_chrom_null(chrom_index, n.metadata['is_null'])
                                num_null += is_null
                                if is_null:
                                    assert not has_data
                            # TODO: remove this condition on time
                            if n.time + 1 < ts.metadata['SLiM']['tick']:
                                assert num_nonmissing + num_null == details['ploidy']
                                assert num_null == details[sex]

