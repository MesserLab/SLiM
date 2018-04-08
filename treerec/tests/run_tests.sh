#!/bin/bash
set -u

# Runs tests under testNoMutationRecipes,
# and then those under testMutationRecipes.

echo "Testing recipes with no mutation."

FILES="./testNoMutationRecipes/test_*.slim ./testMutationRecipes/test_*.slim"

./do_test.sh $FILES
