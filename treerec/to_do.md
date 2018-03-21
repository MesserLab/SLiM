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
- 
