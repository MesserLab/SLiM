# Test suite for tree sequence output from SLiM

## Running the tests
Just do `./run_tests.sh` from within this directory to run the tests on the results of SLiM scripts listed in the `testRecipes/` directory. Alternatively, to run just one of the recipes do `./do_test.sh testRecipes/test_XXX.slim`

## Adding new tests


To add a new test:

1. Put the recipe in the `testRecipes/` directory, with a filename like `test_XXX.slim`.
2. Add the additional necessary stuff:

    * To check that haplotypes agree between SLiM and the tree sequence, just put

      ```
      source("testing_utils.slim");
      ```

      in `initialize()` (this calls `initializeTreeSeq()` and defines functions);
      and then call

      ```
      outputMutationResult();
      ```

      at the end (well, whenever you want, really; but only once).

    * To mark individuals in the initial generation with particular mutation types so we
      can check *something* even with mutation recording turned off, do

      ```
      source("marked_mutations_setup.slim");
	  ```

      in `initialize()`, and then

      ```
      initializeMarks(n_marks);
      ```

      in `1` (after adding the subpop), and then

      ```
      outputMutationTypes();
      ```
      at the end. Note that this will only work properly if there is no new mutation.

    * Add `chooseAncestralSamples(5)` to some generations along the way
      to add some individuals as "ancestral samples". This is done by "remembering" them.

    * If you want to check that individuals in the simulation
      are present in generated tree sequence, call

      ```
      outputIndividuals()
      ```

      By default this simply saves a list of the individuals from the simulation at the point
      when it is called, and compares it to the list in the tree sequence. However, you can
      add individuals to the saved list by calling `addIndividuals(individuals)` at any point
      in the simulation.
      

To temporarily turn off a test, just add e.g. `dont_` to the start of its file name.
Or, add it to `failingTestRecipes/`.

