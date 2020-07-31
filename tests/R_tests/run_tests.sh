#!/bin/bash

for test in test_*.R
do
    Rscript $test &>${test%.R}.log
done
