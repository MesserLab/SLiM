# Notes on implementation

## Individual tracking

Both individuals and nodes (= genomes) have flags. We use individual flags for
internal bookkeeping in SLiM; while msprime uses the SAMPLE flag of nodes.

0. The first generation individuals get entered into the individual table, with
    the `INDIVIDUAL_FIRST_GEN` flag, so we know to keep these around through simplify.
    Their genomes' node IDs are also added to `remembered_genomes_`.

1. Every new individuals' genomes each get the `NODE_IS_SAMPLE` flag,
    because the SAMPLE flag means that we have whole-genome information about a genome,
    which at this point we do. Node times are recorded as the number of generations
    before the start of the simulation (i.e., -1 * sim.generation).

2. Individuals who we call RememberIndividuals on are added to the individual table,
    with the `INDIVIDUAL_REMEMBERED` flag,
    and their genomes' node IDs are added to `remembered_genomes_`.

3. Simplify, if it occurs, first constructs the list of nodes to maintain as samples
    by starting with `remembered_genomes_`, and then adding the genomes of the current
    generation.  All individuals in the individual table should be maintained, because
    they are all pointed to by nodes in this list. It also updates the `msp_node_id_`
    property of the currently alive SLiM genomes. After simplify, only nodes that are
    in `remembered_genomes_` are marked with `NODE_IS_SAMPLE`.

4. On output, we copy the tables, and with these tables,
    add the current generation to the individual table, 
    marked with the `INDIVIDUAL_ALIVE` flag,;
    reorder the individual tables so that the the currently alive ones come first,
    and in the order they currently exist in the population;
    simplify;
    and remove the `NODE_IS_SAMPLE` flag from nodes whose individuals are
    FIRST_GEN but not REMEMBERED or ALIVE.
    Also, node times have the current generation added to them, so they are in units
    of number of generations before the end of the simulation.

5. On input from tables, we use the `INDIVIDUAL_ALIVE` flag to decide who is currently alive,
    and use the `INDIVIDUAL_FIRST_GEN` and `INDIVIDUAL_REMEMBERED` flags to
    decide which genomes to add to `remembered_genomes_`.
    We also add the `NODE_IS_SAMPLE` flag to nodes whose individuals are FIRST_GEN,
    and remove the individuals that are not FIRST_GEN or REMEMBERED (and therefore
    only ALIVE) from the individual table;
    and also remove all ALIVE flags from individuals.
    `msp_node_id_` is set based on the nodes that point to that individual in the tables.
    Also, node times have the current generation subtracted from them, so they are in units
    of number of generations before the start of the simulation.


Internal state:

- `tables.individuals`:

    * who's in the table: FIRST_GEN and/or REMEMBERED
    * flags: everyone is FIRST_GEN and/or REMEMBERED (never ALIVE)

- `tables.nodes`:

    * NODE_IS_SAMPLE flag: **unused** by SLiM; but set if they are in FIRST_GEN
      and/or REMEMBERED individuals, or if they're from a generation since the
      last simplify (or the current generation, if that just happened)

    * individual column: if their individual is FIRST_GEN or REMEMBERED

        - created by AddIndividualsToTable, called by RememberIndividuals and at initialization

    * time: (-1) * birth generation.

- `remembered_genomes_`: the node IDs of genomes whose individuals are FIRST_GEN or REMBEMBERED

    * initialized at the start of each new, not-from-anywhere-else subpopulation
    * added to by RememberIndividuals
    * updated by Simplify

- `msp_node_id_` properties of currently alive SLiM individual `genome1_` and `genome2_`:
    always contains an ID of the node table.

    * created when a new individual is created, by `node_table_add_row()`
    * updated by Simplify

State at output:

- `tables.individuals`:

    * who's in the table: FIRST_GEN and/or REMEMBERED and/or ALIVE
    * flags: everyone is FIRST_GEN and/or REMEMBERED and/or ALIVE
    * reordered by ReorderIndividualTable so that order matches currently alive ones

- `tables.nodes`:

    * NODE_IS_SAMPLE flag: if they are REMEMBERED and/or ALIVE
    * individual column: if they are REMEMBERED and/or ALIVE and/or FIRST_GEN
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

Here's an example of the top-level metadata:
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
