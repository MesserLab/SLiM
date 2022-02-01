"""specify which tests to run on each of the recipes in ./test_recipes"""

import os

# Possible attributes to simulation scripts are
#  mutations/marked_mutations: does the recipe source init.slim or init_marked_mutations.slim? 
#  individuals: records individuals 
#
# All files are of the form `tests/test_recipes/{key}`. By convention a name like
# test_002_xxx is based on the corresponding SLiM test recipe
recipe_specs = {
    "test_____ancestral_marks.slim":	      {"marked_mutations": True},
    "test_____ongoing_muts.slim":		      {"marked_mutations": True},
    "test_____sexual_WF.slim":		          {"marked_mutations": True},
    "test_____sexual_nonwf.slim":		      {"mutations": True},
    "test_____simple_nonwf.slim":		      {"mutations": True},
    "test_____withdraw_indivs.slim":	      {"mutations": True},
    "test_____multipops.slim":	              {"marked_mutations": True, "individuals": True},
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
    "test_____remember_individuals.slim":     {"marked_mutations": True, "individuals": True},
    "test_____retain_individuals_nonWF.slim": {"marked_mutations": True, "individuals": True},
    "test_____retain_individuals_unary.slim": {"marked_mutations": True, "individuals": True},
    "test_____retain_and_remember_individuals.slim": {"marked_mutations": True, "individuals": True},
    "test_____retain_individuals_nonWF_unary.slim": {"marked_mutations": True, "individuals": True},
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
