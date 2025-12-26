# Notes on implementation

## Overall design

To get into how tree-sequence recording in SLiM works, start out looking at SLiM 4.3 (https://github.com/MesserLab/SLiM/releases/tag/v4.3), since that is the last release before SLiM extended to multiple chromosomes (with one tree sequence per chromosome).  The architecture was considerably simpler back then.  You can move up to the SLiM 5.1 sources when you want to see how we handle multiple chromosomes; that topic is also discussed in its own section below.

The relevant variables are mostly in `species.h` (because there is one tree sequence per species, in SLiM 4.3), under the comment `// TREE SEQUENCE RECORDING`.  The most important variable is the table collection,  `tsk_table_collection_t tables_`.  If you search on `tsk_table_collection_` (ignoring hits inside SLiM’s private copy of tskit), you’ll find various interesting spots, such as `Species::AllocateTreeSequenceTables()` where we call `tsk_table_collection_init()` to allocate the table collection, `Species::SimplifyTreeSequence()` where we do a sequence of steps culminating in a call to `tsk_table_collection_simplify()` to simplify the tree sequence, and `Species::FreeTreeSequence()` where we free the table collection with `tsk_table_collection_free()`.  `Species::CheckTreeSeqIntegrity()` calls `tsk_table_collection_check_integrity()` to check that the tree-sequence tables pass all of tskit’s requirements, and is called in various places. `Species::CrosscheckTreeSeqIntegrity()` checks the information in the table collection against the information in SLiM’s own data structures and tries to make sure that they correspond to each other exactly; that’s quite slow and so is mostly done when compiled for debugging, but there is a user-level switch with `initializeTreeSeq(runCrosschecks=T)` that turns it on even in release builds.

A search using the regex `tsk_.*add_row` will turn up places where we record things into various tables.  We build the individual table only at save time, so `tsk_individual_table_add_row()` is called only at that time, notably in `Species::AddIndividualsToTable()` but also in `Species::ReorderIndividualTable()`.  We call `tsk_node_table_add_row()` to record nodes in the node table whenever a new individual is created, in `Species::RecordNewGenome()` (called `RecordNewHaplosome()` in SLiM 5 as a result of the "genome" -> "haplosome" terminology shift that occurred with SLiM 5's multi-chromosome support).  That method also records the edge table entries that describe how that node inherits from ancestral nodes, with `tsk_edge_table_add_row()`.  Notice, by the way, that we check for errors using tskit’s return values after every call we make, and typically call `Species::handle_error()` if there was an error, which uses `tsk_strerror()` to print a user-readable error message; that’s important for catching errors at the point the arise.  `Species::RecordNewDerivedState()` is called whenever a new mutation arises, recording an entry in the site table with `tsk_site_table_add_row()` and an entry in the mutation table with `tsk_mutation_table_add_row()`.  There is considerable strangeness (necessary strangeness) to how SLiM records mutations; you can read about it in chapter 29 of the SLiM manual (again I’d recommend that you start with the 4.3 manual, which you can get from the GitHub release, and only graduate to 5.1 when you’re well-rested :->).  What else?  The population table and an entry for the provenance table are also generated only at save; you can find those place by searching for the appropriate `add_row()` function calls.  There is additional complexity for “remembering” individuals (guaranteeing that they don’t get simplified away); the variable `remembered_genomes_` is the crux of that.  And for “retracting” offspring that get vetoed by a `modifyChild()` callback in the user’s script, the variable `table_position_` records a bookmark that we can roll back to if a proposed child is rejected, in `RetractNewIndividual()`; so it’s the crux of that.

Reading in a `.trees` tree sequence file and creating the corresponding SLiM objects is a complex process that is managed by `Species::_InitializePopulationFromTskitBinaryFile()`, which calls down to a sequence of methods implementing a sequence of sub-steps.  One weirdness to be aware of is that the "derived states" column of the mutation table is required by tskit to be ASCII, but SLiM keeps binary data in it at runtime.  To keep tskit happy, the `Species::DerivedStatesToAscii()` method converts SLiM's binary derived states to ASCII at save time, and the `Species::DerivedStatesFromAscii()` method converts them back to binary at load time.  Speaking of saving, that is done by `Species::WriteTreeSequence()`.

SLiM keeps lots of metadata on the side, in various tables.  That is detailed in chapter 29 of the SLiM manual, "SLiM additions to the .trees file format".  Encoding of metadata is done by the methods `Species::MetadataForMutation()`, `Species::MetadataForSubstitution()`, `Species::MetadataForGenome()`, and `Species::MetadataForIndividual()`.  In SLiM 5 the metadata for genomes (now called haplosomes) got much more complex; you can search on `HaplosomeMetadataRec` in SLiM 5 to find the new code, which has to track which haplosomes are "vacant" and which are not in a multi-chromosome simulation, and this is discussed below in more detail.  SLiM's top-level metadata, including an optional user-supplied metadata dictionary that can be passed to `treeSeqOutput()`, is written by `Species::WriteTreeSequenceMetadata()` and read back in with `ReadTreeSequenceMetadata()`.  SLiM provides schemas for its metadata; they are given in `slim_globals.cpp` beginning with `gSLiM_tsk_metadata_schema`.  In SLiM 4.3 this was complicated by backward compatibility for reading old metadata in SLiM.  In SLiM 5 we abandoned that backward compatibility; the user now needs to use `pyslim` to bring old `.trees` files forward to the current format for reading, which considerably simplified SLiM's code.

That’s most of it.  The remainder of this document will cover more specific implementation details and decisions.


## Individual tracking

Both individuals and nodes (= haplosomes) have flags. We use individual flags for
internal bookkeeping in SLiM; while msprime uses the SAMPLE flag of nodes.

1. When an individual is created in the simulation, its haplosomes (nodes) are automatically
    added to the node table with the `NODE_IS_SAMPLE` flag, because the SAMPLE flag means
    that we have whole-haplosome information about the node, which at this point we do.
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
    flag set in the individuals table, and its haplosomes' node IDs are added to the
    `remembered_nodes_` vector. In contrast, an individual which is retained has the
    `INDIVIDUAL_RETAINED` flag set (which allows round-tripping on input/output).
    However, unless it has also been permanently remembered, a retained individual is
    subject to removal via simplification.

3. Simplify, if it occurs, first constructs the list of nodes to maintain as samples
    by starting with `remembered_nodes_`, and then adding the nodes of the current
    generation.  All permanently remembered individuals in the individual table should be
    maintained, because they are all pointed to by nodes in this list; other individuals
    which have been retained, but not permanently remembered, may get removed by the
    normal simplification algorithm.  We also update the `msp_node_id_` property of the
    currently alive SLiM haplosomes. After simplify, we reset the `NODE_IS_SAMPLE` flag in
    the node table, leaving it set only on those nodes listed in `remembered_nodes_`.
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
    `INDIVIDUAL_REMEMBERED` flag to decide which haplosomes to add to `remembered_nodes_`.
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

- `remembered_nodes_`: the node IDs of nodes whose individuals are REMEMBERED

    * initialized at the start of each new, not-from-anywhere-else subpopulation
    * added to by RememberIndividuals
    * updated by Simplify

- `msp_node_id_` properties of currently alive SLiM individual `haplosome1_` and `haplosome2_`:
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

## Reading in tree sequences

In addition to the various things mentioned above,
To read in a tree sequence, SLiM requires that:

1. the two nodes corresponding to haplosomes of a given individual are adjacent in the node table,
    and sorted by haplosome ID.

This is because of how `__TabulateSubpopulationsFromTreeSequence` works;
probably it could be made more general, but it isn't.

## Multiple chromosomes and multiple tree sequences

In SLiM 5.0 we added support for simulating multiple chromosomes in SLiM, and that added
some wrinkles to how tree-sequence recording is implemented in SLiM.  In particular, we
now keep one tree sequence per chromosome being simulated.  In `species.h` we now define
a struct named `TreeSeqInfo` that keeps the `tsk_table_collection_t`, in particular, as
well as a couple of other bits of information.  Each Species then keeps its own `std::vector`
of `TreeSeqInfo` structs, named `treeseq_`.  Where operations used to involve the table
collection, they now typically involve a loop over the elements of `treeseq_` to perform
the operation on each table collection in turn.

The main complication here is that all of these table collections share three tskit tables:
the node, individual, and population tables.  This means that the table collections all
have a shared structure, and that needs to be preserved across operations like simplify.
The shared tables are kept by the first chromosome's table collection, in `treeseq_[0]`.
The other table collections zero-fill their node, individual, and population tables most
of the time.  That means that, in that state, they are not compliant with tskit
requirements, and trying to use them will often produce a segfault due to a dereference
of a NULL pointer.  That is deliberate and useful; it makes it easy to debug situations
where the table collections are being used when they should not be.  Sometimes we want the
table collections to actually be usable.  For that, `Species::CopySharedTablesIn()` will
do a bitwise, shallow copy of the shared tables into a given table collection; it should
be matched by `DisconnectCopiedSharedTables()` as soon as the operation is done, restoring
the zero-filled table state.  See https://github.com/tskit-dev/tskit/pull/2665 for
Jerome's original multi-chromosome parallel simplification example, from which this design
was derived.

The end goal is that this will allow parallel simplification to happen in SLiM.  The code
in `Species::SimplifyAllTreeSequences()` now implements the extra bookkeeping needed to
maintain the shared table structure, so the design is ready to be parallelized when I
return to the parallelization project.

Single-chromosome simulations still get saved out as a .trees tree sequence file.  With
multiple chromosomes, we now save out a "trees archive" with one .trees file per chromosome.
This is not directly supported on the tskit side at the moment; you can just iterate over
the files in the trees archive and process them as you wish in Python.  An example of this
is provided in the SimHumanity paper, https://doi.org/10.47248/hpgg2505040006.

There is some new top-level metadata associated with trees archives and multiple chromosomes.
This is detailed in section 29.1 of the SLiM manual.  In particular, there are new top-level
keys `this_chromosome` and `chromosomes` that should be provided in every .trees file.  The
`chromosomes` key provides a table of all of the chromosomes involved in the trees archive.
The `this_chromosome` key provides information about the particular chromosome represented
by one particular .trees file in the trees archive.

The haplosome metadata also needs to track which nodes are "vacant" in which chromosomes.  Each individual has two nodes associated with it, and those two nodes are used, in the shared node table, to represent all of the haplosomes for all of the chromosomes for a given individual.  A male individual with a diploid autosome would have two haplosomes for that autosome, and would thus use both node table entries, so the two nodes would not be "vacant" for that autosome.  But an X chromosome in the same male individual would have only one X haplosome, and so it would use only the first of the two nodes for that individual; the second node would be "vacant" for that chromosome in that individual.  The state for the vacant vs. non-vacant state for each node, per chromosome, is kept in the node metadata in SLiM 5.  Section 29.3 of the SLiM manual provides further details and discussion; see also the `remove_vacant()` and `restore_vacant()` methods of `pyslim`.
