"""specify which tests to run on each of the recipes in ./test_recipes"""

import os

# possible attributes to simulation scripts are
#  mutations/marked_mutations: does the recipe source init.slim or init_marked_mutations.slim? 
#  individuals: records individuals 
# All files are of the form `tests/test_recipes/{key}`
recipe_specs = {
    "test_000_ancestral_marks.slim":	      {"marked_mutations": True},
    "test_000_ongoing_muts.slim":		      {"marked_mutations": True},
    "test_000_sexual_WF.slim":		          {"marked_mutations": True},
    "test_000_sexual_nonwf.slim":		      {"mutations": True},
    "test_000_simple_nonwf.slim":		      {"mutations": True},
    "test_000_withdraw_indivs.slim":	      {"mutations": True},
    "test_002_quick_neutral.slim":	          {"marked_mutations": True},
    "test_004 simple sexual A.slim":	      {"mutations": True},
    "test_005 simple sexual X.slim":	      {"mutations": True},
    "test_006 simple sexual Y.slim":	      {"mutations": True},
    "test_007 cloning and selfing.slim":      {"mutations": True},
    "test_008 cyclical subpop.slim":          {"mutations": True},
    "test_009 linear island.slim":            {"mutations": True},
    "test_013 gene conversion.slim":          {"mutations": True},
    "test_024 gene drive.slim":               {"mutations": True},
    "test_038 pure cloning.slim":             {"mutations": True},
    "test_039 pure selfing.slim":             {"mutations": True},
    "test_040 pure cloning sexual.slim":      {"mutations": True},
    "test_042 haploid clonals.slim":          {"mutations": True},
    "test_050_remember_individuals.slim":     {"marked_mutations": True, "individuals": True},
    "test_051_retain_individuals.slim":       {"marked_mutations": True, "individuals": True},
    "test_052_retain_individuals_nonWF.slim": {"marked_mutations": True, "individuals": True},
    "test_097 modeling nucleotides.slim":     {"mutations": True},
}


def recipe_eq(*keys, exclude=None):
    """
    Return an iterator over those recipes whose spec contains the specified keys.
    If key is empty, return all of them.
    If exclude_key is given exclude recipes with the specified key
    """
    if exclude is None:
        return (k for k, v in recipe_specs.items() if all(kk in v for kk in keys))
    else:
        return (
            k for k, v in recipe_specs.items()
            if exclude not in v and all(kk in v for kk in keys)
        )