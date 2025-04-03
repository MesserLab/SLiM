# Configuration for pytest
import collections
import os
import subprocess
import pathlib

from filelock import FileLock
import pytest
import tskit

from recipe_specs import recipe_specs

SLiMindividual = collections.namedtuple('SLiMindividual', 'type population pos nodes')


def load_file_or_dir(p):
    if os.path.isdir(p):
        tsl = [tskit.load(f) for f in pathlib.Path(p).glob("*.trees")]
    else:
        tsl = [tskit.load(p),]
    tsi = [ts.metadata['SLiM']['this_chromosome']['id'] for ts in tsl]
    out = {i : ts for i, ts in zip(tsi, tsl)}
    assert len(out) == len(tsl) # ensures keys are unique
    return out


class OutputResult:
    """
    A wrapper to process the results files returned from a ts
    """
    def __init__(self, out_dir):
        assert os.path.isdir(out_dir)
        self.dir = out_dir

    def get_normal_ts(self):
        ts = tskit.load(os.path.join(self.dir, "test_output.trees"))
        return ts

    def get_ts(self):
        # binary
        print("******** Binary input.")
        bin_ts = load_file_or_dir(os.path.join(self.dir, "test_output.trees"))
        yield bin_ts
        # and nonsimplified binary
        print("******** Unsimplified binary.")
        bin_nonsimp_ts = load_file_or_dir(
            os.path.join(self.dir, "test_output.unsimplified.trees"))
        yield bin_nonsimp_ts

    @staticmethod
    def get_slim_ids(ts):
        # get SLiM ID -> msprime ID map from metadata
        ids = {}
        for n in ts.nodes():
            ids[n.metadata["slim_id"]] = n.id
        return ids

    def mutation_output(self):
        """
        Read in the genotypes output by the SLiM function outputMutationResult(),
        producing a dictionary indexed by position, whose values are dictionaries
        indexed by SLiM genome ID, giving the mutations carried at that position
        in that genome; also return a list of SLiM IDs found.
        """
        basename = "slim_mutation_output.txt"  # matches the name in init.slim
        filename = os.path.join(self.dir, basename)
        if not os.path.isfile(filename):
            raise RuntimeError(f"No mutation file {basename} in {self.dir}")
        slim_file = open(filename, "r")
        slim = {}
        slim_ids = []
        for header in slim_file:
            headstring = header.split()
            assert headstring[0] == "#Genome:"
            genome = int(headstring[1])
            chrom = int(headstring[2])
            # sex = int(headstring[3])
            # ped_id = int(headstring[4])
            slim_ids.append(genome)
            mutations = slim_file.readline().split()
            assert mutations[0] == "Mutations:"
            mutations = [int(u) for u in mutations[1:]]
            positions = slim_file.readline().split()
            assert positions[0] == "Positions:"
            positions = [int(u) for u in positions[1:]]
            for pos, mut in zip(positions, mutations):
                key = (chrom, pos)
                if key not in slim:
                    slim[key] = {}
                if genome not in slim[key]:
                    slim[key][genome] = []
                slim[key][genome].append(mut)
        return slim, slim_ids

    def marked_mutation_output(self, ts):
        """
        This provides a dictionary indexed by chromosome ID,
        whose values are lists indexed by position,
        which contain dicts indexed by mutation type giving the indivs
        inheriting that mut at that position
        """
        basename = "slim_no_mutation_output.txt"  # matches the name in init.slim
        filename = os.path.join(self.dir, basename)
        if not os.path.isfile(filename):
            raise RuntimeError(f"No marked mutation file {basename} in {self.dir}")
        slim_file = open(filename, "r")
        ids = self.get_slim_ids(ts)
        slim = {}
        for header in slim_file:
            assert header[0:12] == "MutationType"
            mut, pos, chrom = header[12:].split()
            chrom = int(chrom)
            if chrom not in slim:
                slim[chrom] = []
            pos = int(pos)
            while pos >= len(slim[chrom]):
                slim[chrom].append({})
            slim_ids = [int(u) for u in slim_file.readline().split()]
            for u in slim_ids:
                assert u in ids
            slim[chrom][pos][mut] = [ids[u] for u in slim_ids]
        return slim

    def individual_output(self):
        # Read in the individuals output by the SLiM function addIndividuals(),
        # producing a dictionary indexed by individual pedigree ID
        basename = "slim_individual_output.txt"  # matches the name in init.slim
        filename = os.path.join(self.dir, basename)
        if not os.path.isfile(filename):
            raise RuntimeError(f"No individual details file {basename} in {self.dir}")
        slim_file = open(filename, "r")
        slim = {}
        for line in slim_file:
            fields = line.split()
            assert fields[0].startswith("#Individual:")
            store = fields[0][len("#Individual:"):]
            assert store in ("remember", "retain", "retain_even_if_unary", "output")
            pedigree_id = int(fields[1])
            if pedigree_id in slim:
                # We have a duplicate; 'remember' takes priority
                if slim[pedigree_id].type == "remember":
                    store = slim[pedigree_id].type
            slim[pedigree_id] = SLiMindividual(
                type=store,
                population=int(fields[2]),
                pos=[float(p) for p in fields[3].split(",")],
                nodes=[int(p) for p in fields[4].split(",")],
            )
        return slim

def make_result(run_dir):
    """
    SLiM recipes are expected to output their results into a set of directories
    within the run_dir: return a list of results, one for each subdirectory
    """
    return [
        OutputResult(os.path.join(run_dir, f.name))
        for f in os.scandir(run_dir)
        if f.is_dir()
    ]

def run_slim(recipe, run_dir, recipe_dir="test_recipes"):
    """
    Run the script specified by recipe (which should switch to the run_dir directory).
    Recipes should dump files into subdirectories of run_dir, and we return a list of
    these directories, which contain the files on which tests should be run
    """
    script_dir = os.path.dirname(os.path.realpath(__file__))  # Path to this file
    full_recipe = os.path.abspath(os.path.join(script_dir, recipe_dir, recipe))
    assert os.path.isdir(run_dir)  # should have been created by caller
    if os.name == "nt":
        run_dir = str(run_dir).replace("\\", "/")
    cmd = ["slim", "-s", "22", "-d", f"RUN_DIR=\"{run_dir}\"", full_recipe]
    print(f"Running {cmd} in dir '{run_dir}', errors etc to 'SLiM_run_output.log'")
    with open(os.path.join(run_dir, "SLiM_run_output.log"), "w") as out:
        retval = subprocess.call(cmd, cwd=script_dir, stderr=subprocess.STDOUT, stdout=out)
        if retval != 0:
            raise RuntimeError(f"Could not run `{cmd}`")
    return make_result(run_dir)


@pytest.fixture(scope="session", params=recipe_specs.keys())
def recipe(request, tmp_path_factory, worker_id):
    """
    Return a dict containing information about the recipe results. Most logic in this
    fixture is to check whether we are running as a single proc, or as a worker.
    """
    recipe_name = request.param
    return_val = {"name": recipe_name}
    return_val.update(recipe_specs[recipe_name])

    if worker_id == "master":
        # not executing in with multiple workers
        run_dir = tmp_path_factory.getbasetemp() / recipe_name
    else:
        # get the temp directory shared by all workers, so that simulation files
        # are shared between workers
        run_dir = tmp_path_factory.getbasetemp().parent / recipe_name
    with FileLock(str(run_dir) + ".lock"):
        # lock until all simulations are done
        if run_dir.is_dir():
            return_val["results"] = make_result(run_dir)
        else:
            os.makedirs(run_dir, exist_ok=True)
            return_val["results"] = run_slim(recipe_name, run_dir)
    return return_val

