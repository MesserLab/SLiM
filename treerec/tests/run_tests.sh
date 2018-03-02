#!/bin/bash

rm -f NodeTable.txt EdgeTable.txt
rm -f TESToutput.txt 

../../bin/slim -s 21 test_ancestral_marks.E

python3 test_ancestral_marks.py
