#!/bin/bash
set -u
FILES=./testRecipes/test_*.slim

for f in $FILES
do
	rm -f test_output/NodeTable.txt test_output/EdgeTable.txt TESToutput.txt SLiM_run_output.log

    echo "Now testing SLiM Recipe: $f"
 	../../bin/slim -s 22 $f &> SLiM_run_output.log
	python3 -m nose test_ancestral_marks.py || exit 1
done


