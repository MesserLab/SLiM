#!/bin/bash
set -eu
FILES=./testRecipes/*.E

for f in $FILES
do
 	../../bin/slim -s 21 $f
	python3 test_ancestral_marks.py

	rm -f NodeTable.txt EdgeTable.txt
	rm -f TESToutput.txt
done

