# Notes on implementation

## Individual tracking

Both individuals and nodes (= genomes) have flags. We use individual flags for
internal bookkeeping in SLiM; while msprime uses the SAMPLE flag of nodes.

1. When an individual is created in the simulation, its genomes (nodes) are automatically
    added to the node table with the `NODE_IS_SAMPLE` flag, because the SAMPLE flag means
    that we have whole-genome information about the node, which at this point we do.
    Node times are recorded as the number of generations before the start of the
    simulation (i.e., -1 * sim.generation). However, the individuals themselves are *not*
    automatically added to the individuals table, in part because much of the information
    in that table (e.g., location, age) may change, so the user should have control over
    when it is recorded. Individuals are only added to the table during output (when all
    the currently alive individuals are added) or if we explictly choose to call the
    RememberIndividuals() function.

2. RememberIndividuals is called with a list of Individual instances. We add each of
    those to to the individual table (but must first check if they are already present).
    If `permanent=T` (default) we remember them permanently, whereas if `permanent=F` we
    simply flag them to be *retained* while their nodes exist in the simulation. When we
    ask an individual to be permanently remembered it has the `INDIVIDUAL_REMEMBERED`
    flag set in the individuals table, and its genomes' node IDs are added to the
    `remembered_genomes_` vector. In constrast, an individual which is retained has the
    `INDIVIDUAL_RETAINED` flag set (which allows round-tripping on input/output).
    However, unless it has also been permanently remembered, a retained individual is
    subject to removal via simplification.

3. Simplify, if it occurs, first constructs the list of nodes to maintain as samples
    by starting with `remembered_genomes_`, and then adding the genomes of the current
    generation.  All permanently remembered individuals in the individual table should be
    maintained, because they are all pointed to by nodes in this list; other individuals
    which have been retained, but not permanently remembered, may get removed by the
    normal simplification algorithm.  We also update the `msp_node_id_` property of the
    currently alive SLiM genomes. After simplify, we reset the `NODE_IS_SAMPLE` flag in
    the node table, leaving it set only on those nodes listed in `remembered_genomes_`.
    Note that we call `simplify` with the `keep_input_roots` option to retain
    lineages back to the first generation, and the `filter_individuals` option to remove
    individuals that no longer need to be retained.

4. On output, we (optionally) simplify, copy the tables, and with these tables:
    add the current generation to the individual table, marked with the `INDIVIDUAL_ALIVE` flag;
    and reorder the individual tables so that the the currently alive ones come first,
    and in the order they currently exist in the population.
    Also, node times have the current generation added to them, so they are in units
    of number of generations before the end of the simulation.
    Note that if simplify is *not* performed before output, all the nodes since the last
    simplify will be marked with the `NODE_IS_SAMPLE` flag, even those not alive. Many of
    these may not have an associated individual. This is deliberate, as we have full
    genomic information for them, even if they are not ALIVE.

5. On input from tables, we use the `INDIVIDUAL_ALIVE` flag to decide who is currently
    alive and therefore must be instantiated into a SLiM individual, and use the
    `INDIVIDUAL_REMEMBERED` flag to decide which genomes to add to `remembered_genomes_`.
    From the individual table, we remove all individuals apart from those that are marked
    as either `INDIVIDUAL_REMEMBERED` or `INDIVIDUAL_RETAINED` (thus we remove from the
    table individuals that were only exported in the first place because they were ALIVE
    at the time of export). `msp_node_id_` is set based on the nodes that point to that
    individual in the tables. Also, node times have the current generation subtracted
    from them, so they are in units of number of generations before the start of the simulation.

Internal state:

- `tables.individuals`:

    * who's in the table: REMEMBERED and (if they have an existing node) RETAINED
    * flags: everyone either REMEMBERED or RETAINED or both (never ALIVE)

- `tables.nodes`:

    * NODE_IS_SAMPLE flag: **unused** by SLiM; but set if they are in
      REMEMBERED individuals, or if they're from a generation since the
      last simplify (or the current generation, if that just happened)

    * individual column: if their individual is REMEMBERED or RETAINED

        - created by AddIndividualsToTable, called by RememberIndividuals

    * time: (-1) * birth generation.

- `remembered_genomes_`: the node IDs of genomes whose individuals are REMEMBERED

    * initialized at the start of each new, not-from-anywhere-else subpopulation
    * added to by RememberIndividuals
    * updated by Simplify

- `msp_node_id_` properties of currently alive SLiM individual `genome1_` and `genome2_`:
    always contains an ID of the node table.

    * created when a new individual is created, by `node_table_add_row()`
    * updated by Simplify

State at output:

- `tables.individuals`:

    * who's in the table: ALIVE and/or REMEMBERED and/or (if node exists) RETAINED
    * flags: everyone is ALIVE and/or REMEMBERED and/or RETAINED
    * reordered by ReorderIndividualTable so that order matches currently alive ones

- `tables.nodes`:

    * NODE_IS_SAMPLE flag: if they are REMEMBERED and/or ALIVE, or all nodes since the
       last simplification if output has been done with simplify=F
    * individual column: if they are REMEMBERED and/or ALIVE
    * time: (current generation - birth generation)


## Mutation recording

### Alleles

We record as the `derived_state` for each mutation the
*concatenation* of all mutations at that site that the individual has.
This is necessary because stacking rules can change dynamically,
and makes sense, because this is what the individual actually passes on to offspring.

### Sites and mutation parents

Whenever a new mutation is encountered, we do the following:

1. Add a new `site` to the site table at this position.
2. Add a new `mutation` to the mutation table at the newly created site.

This is lazy and wrong, because:

a. There might have already been sites in the site table with the same position,
b. and/or a mutation (at the same position) that this mutation should record as it's `parent`.

But, it's all OK because here's what we do:

1. Add rows to the mutation and site tables as described above.
2. Periodically, `sort_tables`, `deduplicate_sites`,  and `simplify_tables`, then return to (1.), except that
3. Sometimes, to output the tables, we `sort_tables`, `compute_parents`,
    (optionally `simplify_tables`), and dump these out to a file.

*Note:* as things are going along we do *not* have to `compute_parents`;
this only has to happen before the final (output) step.
It might also be possible to skip the `deduplicate_sites` as we go along,
if `simplify_tables` is OK with more than one site at the same position.

And, these operations have the following properties:

1. Mutations appear in the mutation table ordered by time of appearance,
    so that a mutation will always appear after the one that it replaced (i.e., it's parent).
2. Furthermore, if mutation B appears after mutation A, but at the same site, 
    then mutation B's site will appear after mutatation A's site in the site table.
3. `sort_tables` sorts sites by position, and then by ID, so that the relative ordering of sites
    at the same position is maintained, thus preserving property (2).
4. `sort_tables` sorts mutations by site, and then by ID, thus preserving property (1);
    if the mutations are at separate sites (but the same position),
    this fact is thanks to property (2).
5. `simplify_tables` also preserves ordering of any rows in the site and mutation tables
    that do not get discarded.
5. `deduplicate_sites` goes through and collapses all sites at the same position to only one site,
    maintaining order otherwise.
6.  `compute_parents` fills in the `mutation.parent` information by using property (1).


## Population table state

SLiM needs to be able to have an entry in the population table without individuals
actually mean that there's an empty population (for nonWF models),
and so we (now as of SLiM v4.0) have the requirement that all the populations
in the population table that have valid SLiM metadata (which currently just means
having a "slim_id" key) must represent actual extant subpopulation in SLiM.
In other words, the state of the population table represents the state of things
*at the time the tree sequence is written*, although it might have other,
non-SLiM entries in it which got carried over by SLiM from a tree sequence
that was read in.

We also require the "slim_id" key in metadata to match the row of the population table
in which it is found. The reason for this is because we may refer to (sub)population IDs
in several places: (a) the population column of the node table; (b) the subpopulation
entry of the individual metadata; (c) the subpopulation entry of the mutation metadata.
Of these, (a) is exposed to tskit (and so will remain consistent under tskit operations)
but (b) and (c) are not. However, SLiM will ensure that (a), (b), and (c) remain consistent.
It would be possible to decouple row of the population table with "slim_id" entry in metadata,
by simply declaring that things referred to in metadata refer to the slim_id of a population's
metadata, while the population column in tables refers to the population id as usual.
However, this would then require substantial downstream consistency checking:
for instance, individual->node->population.metadata["slim_id"] should always
equal individual.metadata["subpopulation"] . We would also need to ensure that the slim_id's
were always unique.

## Metadata schemas

tskit provides methods for structured metadata decoding using [JSON schemas](https://json-schema.org/understanding-json-schema/index.html),
to document what the metadata means.
We don't make use of these, but write them to the tree sequence for tskit use.
There's both top-level metadata (ie for the whole tree sequence)
and metadata for every row in every table.
(But, we only use some of these.)

For practical purposes, the metadata schema can be any equivalent JSON.
However, when checking for table equality,
the code checks whether the underlying string representations are equal.
So, we want the metadata schema as written by SLiM to match that written by pyslim (using tskit).
The way that tskit writes out a metadata schema to a tree sequence
is by (a) defining the schema using a dict
and (b) creating a string using `json.dumps(schema_dict, sort_keys=True, indent=4)`.
Happily, the resulting text matches the output of nlohmann::json dump(4),
so - it seems - we can merrily write out JSON ourselves.
Nonetheless, we only actually do JSON parsing and writing in SLiM's code for top-level metadata:
for all the schemas (including the top-level metadata schema)
we just write out the string representation, as output by tskit, saved in `slim_globals.h`.

In the future we may want to *keep* whatever top-level metadata there is
in the tree sequence already (and the associated keys in the schema).
We've not done that yet because making things exactly match seems like a pain,
and no-one else is using the top-level metadata yet.

### Top-level metadata:

*Note: this is out of date.* Here's an example of the top-level metadata:
```
{
 "SLiM" : {
     "model_type" : "WF",
     "generation" : 123,
     "file_version" : "0.5",
     "spatial_dimensionality" : "xy",
     "spatial_periodicity" : "x",
     "separate_sexes" : true,
     "nucleotide_based" : false
 }
}
```

However, we're currently only using `model_type`, `generation`, and `file_version`.
