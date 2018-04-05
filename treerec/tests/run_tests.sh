#!/bin/bash
set -u
FILES=./testRecipes/test_*.slim

mkdir -p test_output

for f in $FILES
do
	rm -f test_output/*

    echo "Now testing SLiM Recipe: $f"
 	../../bin/slim -s 22 $f &> test_output/SLiM_run_output.log
	python3 -m nose test_ancestral_marks.py || exit 1
done


