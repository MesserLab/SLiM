# Notes on implementation

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

