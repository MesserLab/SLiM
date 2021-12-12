# Test suite for tree sequence output from SLiM

## Running the tests
Just do `python3 -m pytest` from within this directory to run the tests on the results of SLiM scripts listed in the `testRecipes/` directory. Alternatively, to run just one of the recipes do e.g.
`python3 -m pytest -k test_000_sexual_nonwf`

## Recaching in GitHub Actions to get a new tskit/msprime version
GitHub Actions caches its install of `tskit`, `msprime`, and other software.  When a new version of such software is released, a recache needs to be forced or these tests will likely fail in CI.  This cannot presently be gone in GitHub's UI; see [this GitHub issue](https://github.com/actions/cache/issues/2).  So to trigger a recache, you need to increment the cache version number.  It is found in `.github/workflows/tests.yml` in the line:

`key: ${{ runner.os }}-${{ matrix.python}}-conda-v10-${{ hashFiles('treerec/tests/conda-requirements.txt') }}-${{ hashFiles('treerec/tests/pip-requirements.txt') }}`

For example, change `v10` here to `v11`.  You can also consider putting an explicit version number in `treerec/tests/conda-requirements.txt`; for example, we now have `tskit >= 0.4.0` there, although presumably it would get the most recent shipping version anyway; I guess this expresses the version requirement semantically, for human readers, at least.  Changing either the version number or the `conda-requirements.txt` should apparently trigger a recache.  See [this issue](https://github.com/MesserLab/SLiM/issues/232) for the context that led to this comment.

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
