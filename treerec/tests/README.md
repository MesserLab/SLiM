# Test suite for tree sequence output from SLiM

## Running the tests
Just do `python3 -m pytest` from within this directory to run the tests on the results of SLiM scripts listed in the `testRecipes/` directory. Alternatively, to run just one of the recipes do e.g. 
`python3 -m pytest -k test_000_sexual_nonwf`

## Adding new tests


To add a new test:

1. Put the recipe in the `test_recipes/` directory, with a filename like `test_XXX.slim`.
2. Add a config line in `recipe_specs.py` which determines which tests to run.
3. Add the additional necessary stuff to the SLiM recipe:

    * To check that haplotypes agree between SLiM and the tree sequence, just put

      ```
      source("init.slim");
      ```

      in `initialize()` (this defines functions); and then call

      ```
      outputMutationResult();
      ```

      at the end (well, whenever you want, really;).

    * To mark individuals in the initial generation with particular mutation types so we
      can check *something* even with mutation recording turned off, instead of sourcing
      "init.slim", source "init_marked_mutations.slim", i.e. place

      ```
      source("init_marked_mutations.slim");
	  ```

      in `initialize()` and then call

      ```
      initializeMarks(n_marks);
      ```

      in `1` (after adding the subpop), and then, as before,

      ```
      outputMutationResult();
      ```
      at the end. Note that this will only work properly if there is no new mutation.

    * Make sure you call `initializeTreeSeq();` after the `source(...)` line

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
      

To temporarily turn off a test just comment out the appropriate line in `recipe_specs.py`

