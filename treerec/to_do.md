Overall:

- add hooks for mutations : BH
- add mutation recording : PR
- automatic simplify interval recording : PR

    * keep track of a between-simplify interval; adjust it to get a target pre:post table size ratio

- API additions : BH

    * `treeSeqSimplify()`
    * `treeSeqRememberIndividuals(o<Ind> inds)` : record ancestral samples
    * input of prior history through tables (somehow)
    * `treeSeqOutput(s$ path, l$ simplify=T, l$binary=T)` : note, illegal to call at certain points mid-generation
    * `initializeTreeSeq(f$ simplifyRatio=2, l$recordMutations=?)`

- add tree sequence whatnot for including prior history
- make decisions about metadata

    * nodes: should know if genome was maternally or paternally inherited
    * mutations: 

- efficiency: don't record (or, un-record) individuals discarded by `modifyChild`


Mutations:

- record as derived state the sequence of mutation IDs (which are 64bit integers), embedded in the character array

- write a python script that will take those messy derived states and 

    * move the list of mutation IDs over into metadata
    * generate ACGT or something from these instead

- for C-level testing, use a [`vargen`](https://github.com/tskit-dev/msprime/blob/master/lib/vargen.c),
    which gives you an iterator over `variants`, which include a list of `mutations`, as well as a vector of `genotypes`,
    which are indices into the list of mutations of which mutation each individual carries.


