import pyslim
import msprime

class TestSlimOutput:

    def get_slim_ids(self, ts):
        # get SLiM ID -> msprime ID map from metadata
        ids = {}
        for n in ts.nodes():
            ids[n.metadata["slim_id"]] = n.id
        return ids

    def get_ts(self):
        if False:
            # read in from text
            node_file = open("test_output/NodeTable.txt", "r")
            edge_file = open("test_output/EdgeTable.txt", "r")
            try:
                site_file = open("test_output/SiteTable.txt", "r")
            except IOerror:
                site_file = None
            try:
                mutation_file = open("test_output/MutationTable.txt", "r")
            except IOerror:
                mutation_file = None
            try:
                individual_file = open("test_output/IndividualTable.txt", "r")
            except IOerror:
                individual_file = None
            try:
                population_file = open("test_output/PopulationTable.txt", "r")
            except IOerror:
                population_file = None
            text_ts = msprime.load_text(nodes=node_file, edges=edge_file, 
                                   sites=site_file, mutations=mutation_file,
                                   individuals=individual_file,
                                   populations=population_file,
                                   base64_metadata=False)

            print("******* Text input.")
            yield text_ts
        # and binary
        bin_ts = pyslim.load("test_output/test_output.trees")
        print("******** Binary input.")
        yield bin_ts
        # and nonsimplified binary
        print("******** Unsimplified binary.")
        bin_nonsip_ts = pyslim.load("test_output/test_output.unsimplified.trees")
        yield bin_nonsip_ts
