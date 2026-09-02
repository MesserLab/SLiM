import tskit, pyslim
import numpy as np
import pytest
from collections import Counter

from recipe_specs import recipe_eq

class TraitCalculator:
    """
    Calculating phenotypes by hand from metadata takes a good bit of
    internal state, especially if it's from a multi-chromosome simulation,
    so here's a class that does it.  See below for how to use this.
    """

    def __init__(self, ts):
        self.ts_metadata = None
        self.slim_phenotypes = None
        self.offsets = None
        self.add_ts(ts)

    def add_ts(self, ts):
        ts_metadata = ts.metadata
        if self.ts_metadata is not None:
            assert ts_metadata["SLiM"]["traits"] == self.ts_metadata["SLiM"]["traits"]
        self.ts_metadata = ts_metadata
        self.ts = ts
        chrom_type = self.ts_metadata["SLiM"]["this_chromosome"]["type"]
        self.haploid_chromosome = chrom_type in ("H", "HF", "HM")
        self.mut_metadata = pyslim.mutation_metadata(ts)
        self.nodes_vacant = pyslim.nodes_vacant(ts)
        self.num_traits = len(self.ts_metadata["SLiM"]["traits"])
        self.types = [m["type"] for m in self.ts_metadata["SLiM"]["traits"]]
        self.accumulates = [
            m["baselineAccumulation"] for m in self.ts_metadata["SLiM"]["traits"]
        ]
        # to properly do traits in the case where baselineAccumulation=F and there are
        # mutation types with convertToSubstitution=T then we need to store this in metadata:
        # (so, this code won't work if this is not present for such traits, which are probably
        # only the default trait!)
        if (
            "user_metadata" in ts_metadata["SLiM"]
            and "mutation_types" in ts_metadata["SLiM"]["user_metadata"]
        ):
            self.mutation_types = {
                k: v[0]["convertToSubstitution"][0]
                for k, v in ts_metadata["SLiM"]["user_metadata"]["mutation_types"][
                    0
                ].items()
            }
        else:
            # guess T if there just a single trait without baseline accumulation
            # whose name is simT and this is a WF model
            # since that describes "default trait"
            convert = (
                self.ts_metadata["SLiM"]["model_type"] == "WF"
                and len(self.ts_metadata["SLiM"]["traits"]) == 1
                and self.ts_metadata["SLiM"]["traits"][0]["name"] == "simT"
                and self.ts_metadata["SLiM"]["traits"][0]["baselineAccumulation"]
                == False
            )
            print("convert!", convert)
            mtypes = set(md["mutation_type"] for md in self.mut_metadata.values())
            self.mutation_types = {f"m{k}": convert for k in mtypes}
        sp = np.column_stack(
            [
                self.ts.tables.individuals.metadata_vector(["per_trait", j, "phenotype"])
                for j in range(self.num_traits)
            ]
        )
        if self.slim_phenotypes is None:
            self.slim_phenotypes = sp
        assert np.all(self.slim_phenotypes == sp)
        # initialize phenotypes where we'll accumulate changes
        self.phenotypes = np.full(
            self.slim_phenotypes.shape,
            [1.0 if t == "multiplicative" else 0.0 for t in self.types],
            dtype="float",
        )
        offsets = np.column_stack(
            [
                self.ts.tables.individuals.metadata_vector(["per_trait", j, "offset"])
                for j in range(self.num_traits)
            ]
        )
        if self.offsets is None:
            self.offsets = offsets
            # only do offsets if this is the first ts!
            self.do_offsets()
        else:
            assert np.all(offsets == self.offsets)
        self.get_frequencies()
        self.do_effects()

    def transform(self):
        for k, t in enumerate(self.types):
            if t == "logistic":
                self.phenotypes[:, k] = 1 / (1 + np.exp(-self.phenotypes[:, k]))

    def is_converted(self, a):
        # is allele a from a convert-to-substitution mutation type?
        # if we haven't recorded whether or not it's converted, assume T
        md = self.mut_metadata[int(a)]
        return self.mutation_types[f"m{md['mutation_type']}"]

    def get_frequencies(self):
        # Fixed mutations count unless they are converted to substitutions
        # and baseline accumulation is off, so we need to look up both these things:
        # (frequency, whether they're converted to substitutions) for each mutation.
        self.frequencies = {}
        for v in self.ts.variants(isolated_as_missing=False):
            freqs = {}
            for ds, f in v.counts().items():
                for a in ds.split(","):
                    if a != "":
                        if a not in freqs:
                            convert = self.is_converted(a)
                            freqs[a] = [0, convert]
                        freqs[a][0] += f
            self.frequencies[v.site.id] = freqs

    def do_offsets(self):
        for k, t in enumerate(self.types):
            bO = self.ts_metadata["SLiM"]["traits"][k]["baselineOffsetFromUser"]
            if t == "multiplicative":
                self.phenotypes[:, k] *= bO * self.offsets[:, k]
            else:
                self.phenotypes[:, k] += bO + self.offsets[:, k]

    def do_effects(self):
        for ind in self.ts.individuals():
            ploidy = np.sum(~self.nodes_vacant[ind.nodes])
            if ploidy == 0:
                continue
            else:
                hemizygous = ploidy == 1
                for k, t in enumerate(self.types):
                    if t == "multiplicative":
                        g = self.multiplicative_effect(ind, k, hemizygous)
                        self.phenotypes[ind.id, k] *= g
                    else:
                        g = self.additive_effect(ind, k, hemizygous)
                        self.phenotypes[ind.id, k] += g

    def skip_mut(self, m, freqs, trait_id):
        if m == "":
            out = True
        else:
            f, convert = freqs[m]
            out = f == 0 or (f == self.ts.num_samples and convert and not self.accumulates[trait_id])
        return out

    def additive_effect(self, ind, trait_id, hemizygous):
        out = 0.0
        for v in self.ts.variants(samples=ind.nodes, isolated_as_missing=False):
            freqs = self.frequencies[v.site.id]
            a = ",".join([v.alleles[g] for g in v.genotypes])
            muts = Counter(a.split(","))
            for m in muts:
                if self.skip_mut(m, freqs, trait_id):
                    continue
                md = self.mut_metadata[int(m)]["per_trait"][trait_id]
                s = md["effect_size"]
                if muts[m] == 2:
                    h = 1
                else:
                    assert muts[m] == 1
                    if self.haploid_chromosome:
                        h = 1
                    else:
                        if hemizygous:
                            h = md["hemizygous_dominance"]
                        else:
                            h = md["dominance"]
                        if np.isnan(h):
                            h = 1 / 2
                out += 2 * h * s
        return out

    def multiplicative_effect(self, ind, trait_id, hemizygous):
        out = 1.0
        for v in self.ts.variants(samples=ind.nodes, isolated_as_missing=False):
            freqs = self.frequencies[v.site.id]
            a = ",".join([v.alleles[g] for g in v.genotypes])
            muts = Counter(a.split(","))
            for m in muts:
                if self.skip_mut(m, freqs, trait_id):
                    continue
                assert muts[m] > 0 and muts[m] <= 2
                md = self.mut_metadata[int(m)]["per_trait"][trait_id]
                s = md["effect_size"]
                if muts[m] == 2:
                    h = 1
                else:
                    assert muts[m] == 1
                    if self.haploid_chromosome:
                        h = 1
                    elif hemizygous:
                        h = md["hemizygous_dominance"]
                    else:
                        h = md["dominance"]
                    if np.isnan(h):
                        # "independent dominance occurs when (1+hs)(1+hs) equals 1+s,
                        # which occurs when h=(sqrt(1+s)−1)/s"
                        h = (np.sqrt(1 + s) - 1) / s if s != 0 else 0
                out *= 1 + h * s
        return out


class TestTraits:

    @pytest.mark.parametrize("recipe", recipe_eq("traits"), indirect=True)
    def test_traits_consistency(self, recipe):
        assert len(recipe["results"]) > 0
        for result in recipe["results"]:
            # note we don't have multi-chromosome sims here!
            # see pyslim tests for that setup
            ts = result.get_normal_ts()
            assert ts.num_mutations > 0
            assert ts.num_individuals > 0
            # phenotypes as computed by us
            tc = TraitCalculator(ts)
            tc.transform()
            print(np.column_stack([tc.phenotypes, tc.slim_phenotypes,
                                   tc.phenotypes - tc.slim_phenotypes]))
            assert np.allclose(tc.phenotypes, tc.slim_phenotypes)

