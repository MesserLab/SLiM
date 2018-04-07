#!/bin/bash
set -u

# Runs tests under testNoMutationRecipes,
# and then those under testMutationRecipes.

echo "Testing recipes with no mutation."

FILES=./testNoMutationRecipes/test_*.slim

mkdir -p test_output

for f in $FILES
do
	rm -f test_output/*

    echo "Now testing SLiM Recipe: $f"
 	../../bin/slim -s 22 $f &> test_output/SLiM_run_output.log
	python3 -m nose test_no_mutation_output.py || exit 1
done


echo "Testing recipes with mutation."

FILES=./testMutationRecipes/test_*.slim

mkdir -p test_output

for f in $FILES
do
	rm -f test_output/*

    echo "Now testing SLiM Recipe: $f"
 	../../bin/slim -s 22 $f &> test_output/SLiM_run_output.log
	python3 -m nose test_mutation_output.py || exit 1
done



