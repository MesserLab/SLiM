import tskit, msprime, pyslim
import numpy as np
import pytest

class TestSimple:
    # Some very very simple tests that are designed to be easier to track down
    # than the exhaustive make-sure-everything-matches tests.

    @pytest.mark.parametrize('recipe', ["test_____simple_none.slim"], indirect=True)
    def test_simple_not_remembered(self, recipe):
        # this is a recipe that has 4 individuals, runs for 4 ticks.
        assert len(recipe["results"]) == 1
        ts = recipe["results"][0].get_normal_ts()
        assert np.max(ts.nodes_time) == 4.0
        assert len(ts.samples(time=0.0)) == 8
        for t in np.arange(1, 6):
            assert len(ts.samples(time=t)) == 0

    @pytest.mark.parametrize('recipe', ["test_____simple_not_perm.slim"], indirect=True)
    def test_simple_remembered_not_perm(self, recipe):
        # this is a recipe that has 4 individuals, runs for 4 ticks,
        # and remembers individuals not-permanently at tick 2
        assert len(recipe["results"]) == 1
        ts = recipe["results"][0].get_normal_ts()
        assert np.max(ts.nodes_time) == 4.0
        assert len(ts.samples(time=0.0)) == 8
        for t in np.arange(1, 6):
            assert len(ts.samples(time=t)) == 0

    @pytest.mark.parametrize('recipe', ["test_____simple_perm.slim"], indirect=True)
    def test_simple_remembered_perm(self, recipe):
        # this is a recipe that has 4 individuals, runs for 4 ticks,
        # and remembers individuals permanently at tick 2
        assert len(recipe["results"]) == 1
        ts = recipe["results"][0].get_normal_ts()
        assert np.max(ts.nodes_time) == 4.0
        assert len(ts.samples(time=0.0)) == 8
        for t in np.arange(1, 6):
            assert len(ts.samples(time=t)) == (0 if t != 2.0 else 8)

