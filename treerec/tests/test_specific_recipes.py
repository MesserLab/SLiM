"""
Tests that look at the result of specific recipes in test_recipes
These may depend on the exact random seed used to run the SLiM simulation.
Individual recipes can be specified by decorating with
@pytest.mark.parametrize('recipe', ['test_____my_specific_recipe.slim'], indirect=True)
"""
import msprime
import numpy as np
import pyslim
import pytest

from recipe_specs import recipe_eq


class TestUnaryNodes:

    def max_children_node(self, ts, exclude_roots=True, exclude_samples=True):
        """
        Return a dict mapping nodes to their maximumn # children in the ts
        """

        max_children = np.zeros(ts.num_nodes, dtype=int)
        filter = np.zeros(ts.num_nodes, dtype=bool)
        if exclude_samples:
            filter[ts.samples()] = True
        if exclude_roots:
            filter[np.isin(np.arange(ts.num_nodes), ts.tables.edges.child) == False] = True
        for tree in ts.trees():
            for n in tree.nodes():
                if not ts.node(n).is_sample():
                    max_children[n] = max(max_children[n], tree.num_children(n))
        arr = {n:mc for n, (mc, rm) in enumerate(zip(max_children, filter)) if not rm}
        return arr
        

    @pytest.mark.parametrize('recipe', indirect=True, argvalues=[
        "test_____retain_individuals_nonWF_unary.slim",
        "test_____retain_individuals_unary.slim",
    ])
    def test_contains_unary_nonsample_nodes(self, recipe):
        for result in recipe["results"]:
            for ts in result.get_ts():
                assert np.any(np.array(list(self.max_children_node(ts).values())) == 1)

    @pytest.mark.parametrize('recipe', indirect=True, argvalues=[
        'test_____remember_individuals.slim',
    ])
    def test_contains_unary_sample_nodes(self, recipe):
        """
        Historical remembered individuals can have a node with a single descendant
        """
        for result in recipe["results"]:
            for ts in result.get_ts():
                num_unary_nodes = 0
                for tree in ts.trees():
                    for n in tree.nodes():
                        if tree.num_children(n) == 1 and tree.parent(n) >= 0:
                            assert ts.node(n).individual >= 0  # has an individual
                            ind = ts.individual(ts.node(n).individual)
                            assert ts.node(n).is_sample()
                            assert (ind.flags & pyslim.INDIVIDUAL_REMEMBERED) != 0
                            num_unary_nodes += 1
                assert num_unary_nodes > 0

    @pytest.mark.parametrize('recipe', indirect=True, argvalues=[
        "test_____retain_and_remember_individuals.slim",
        "test_____retain_individuals_nonWF.slim",
        "test_____remember_individuals.slim",
    ])
    def test_no_purely_unary_internal_nonsample_nodes(self, recipe):
        for result in recipe["results"]:
            for ts in result.get_ts():
                max_children_per_node = self.max_children_node(ts)
                assert np.all(np.array(list(max_children_per_node.values())) != 1)


class TestIndividualsInGeneration:
    """
    Test that all the nodes on the ancestry at a fixed set of generations are present
    """
    def num_lineages_at_time(self, ts, focal_time, pos):
        edges = ts.tables.edges
        times = ts.tables.nodes.time
        return np.sum(np.logical_and.reduce((
            times[edges.parent] > focal_time,
            times[edges.child] <= focal_time,
            edges.left <= pos,
            edges.right > pos,
        )))
        

    @pytest.mark.parametrize('recipe, gens, final_gen', indirect=["recipe"], argvalues=[
        ('test_____retain_individuals_unary.slim', (50, 100), 200),
    ])
    def test_all_lineages_covered(self, recipe, gens, final_gen):
        """
        If all individuals in `gen` are retained with retainCoalescentOnly=F, we should
        have unary nodes in individuals for all tree sequence lineages
        """
        gens = final_gen - np.array(gens)  # convert to ts times
        for result in recipe["results"]:
            for ts in result.get_ts():
                # Check all initial generation nodes are roots
                nodes_at_start = np.where(ts.tables.nodes.time == final_gen)[0]
                nodes_at_gen = {g: np.where(ts.tables.nodes.time == g)[0] for g in gens}
                for nodes in nodes_at_gen.values():
                    # All nodes in the target generations should have an individual
                    assert np.all(ts.tables.nodes.individual[nodes] >= 0)
                for tree in ts.trees():
                    assert np.all(np.isin(tree.roots, nodes_at_start))
                    for gen, nodes in nodes_at_gen.items():
                        treenodes_at_gen = set(tree.nodes()) & set(nodes)
                        assert len(treenodes_at_gen) == self.num_lineages_at_time(
                            ts, gen, tree.interval.left)

    @pytest.mark.parametrize('recipe, gens, final_gen', indirect=["recipe"], argvalues=[
        ('test_____retain_and_remember_individuals.slim', (50, 100), 200),
    ])
    def test_not_all_lineages_covered(self, recipe, gens, final_gen):
        """
        If all individuals in `gen` are retained with retainCoalescentOnly=T, we should
        simplify out those with only unary nodes, and not all lineages will have a node
        """
        gens = final_gen - np.array(gens)  # convert to ts times
        for result in recipe["results"]:
            for ts in result.get_ts():
                # Check all initial generation nodes are roots
                nodes_at_start = np.where(ts.tables.nodes.time == final_gen)[0]
                nodes_at_gen = {g: np.where(ts.tables.nodes.time == g)[0] for g in gens}
                for nodes in nodes_at_gen.values():
                    # All nodes in the target generations should have an individual
                    assert np.all(ts.tables.nodes.individual[nodes] >= 0)
                for tree in ts.trees():
                    assert np.all(np.isin(tree.roots, nodes_at_start))
                    for gen, nodes in nodes_at_gen.items():
                        treenodes_at_gen = set(tree.nodes()) & set(nodes)
                        assert len(treenodes_at_gen) < self.num_lineages_at_time(
                            ts, gen, tree.interval.left)
