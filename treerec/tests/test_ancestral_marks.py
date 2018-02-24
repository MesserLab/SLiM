import msprime

def check_consistency(x, y):
    # here y is an iterable of disjoint lists of ids (determined by indivs who share mutations in SLiM)
    # and x is a dict whose keys are ids and whose values are labels (the roots in msprime);
    # check that any two ids in the same element of y have the same label in x
    print("x:", x)
    print("y:", y)
    all_ids = []
    for a in y:
        if len(y[a]) > 0:
            a_labels = []
            for u in y[a]:
                print("u:", u)
                assert(u in x)
                assert(u not in all_ids)
                all_ids.append(u)
                a_labels.append(x[u])
            print('al:', a_labels)
            assert(len(set(a_labels)) == 1)


# load tree sequence
node_file = open("NodeTable.txt", "r")
edge_file = open("EdgeTable.txt", "r")
ts = msprime.load_text(nodes=node_file, edges=edge_file, decode_metadata=False)

# get msprime -> SLiM ID map from metadata
ids = {}
for n in ts.nodes():
    meta = n.metadata.decode('utf8')
    assert(meta[:7] == "SLiMID=")
    ids[n.id] = int(n.metadata.decode('utf8').strip().split("=")[1])

# iterate through SLiM output information
slim_file = open("TESToutput.txt", "r")

# slim will be indexed by position,
# and contain a dict indexted by mutation type giving the indivs
# inheriting that mut at that position
slim = []

for header in slim_file:
    assert(header[0:12] == "MutationType")
    mut, pos = header[12:].split()
    pos = int(pos)
    if pos == len(slim):
        slim.append({})
    slim[pos][mut] = [int(u) for u in slim_file.readline().split()]


# test these agree
pos = 0
for t in ts.trees():
    # get partition of leaves from this tree, using SLiM IDs
    fams = {}
    for x in t.leaves():
        u = x
        while t.parent(u) != msprime.NULL_NODE:
            u = t.parent(u)
        fams[ids[x]] = u
    while pos < t.interval[1]:
        assert(check_consistency(fams, slim[pos]))
        pos += 1

