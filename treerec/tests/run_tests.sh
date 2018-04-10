#!/bin/bash
set -u

# Runs tests under testRecipes/

FILES="./testRecipes/test_*.slim"

./do_test.sh $FILES
