#!/bin/bash
set -u

USAGE="Usage:
    $0 (name of slim recipe to run)
"

if [ $# -lt 1 ];
then
    echo $USAGE
    exit 0
fi

mkdir -p test_output

for RECIPE in "$@"
do
    if [ ! -e "$RECIPE" ]
    then
        echo "Recipe $RECIPE does not exist."
        exit 1
    fi

	rm -rf test_output/*

    echo "Now testing SLiM Recipe: $RECIPE"
    echo "Running:"
    echo "slim -s 22 $RECIPE &> test_output/SLiM_run_output.log"
    slim -s 22 "$RECIPE" &> test_output/SLiM_run_output.log || (echo "SLiM error" && exit 1)

    TESTED="no"

    if [ -e "test_output/slim_no_mutation_output.txt" ]
    then
        echo "  Testing no-mutation output."
        python3 -m nose test_no_mutation_output.py || exit 1
        TESTED="yes"
    fi

    if [ -e "test_output/slim_mutation_output.txt" ]
    then
        echo "  Testing with-mutation output."
        python3 -m nose test_mutation_output.py || exit 1
        TESTED="yes"
    fi

    if [ "$TESTED" = "no" ]
    then
        echo "    ERROR: no test output."
        exit 1
    fi

done

exit 0
