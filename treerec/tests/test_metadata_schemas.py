"""
Tests for metadata schema stuff.
Individual recipes can be specified by decorating with
@pytest.mark.parametrize('recipe', ['test_____my_specific_recipe.slim'], indirect=True)
"""
import json
import tskit
import msprime
import pyslim
import pytest

from recipe_specs import recipe_eq

table_names = [
    'edges', 'individuals', 'migrations', 'mutations',
    'nodes', 'populations', 'sites'
]

# this is for indexing pyslim.slim_metadata_schemas
table_namex = [
    'edge', 'individual', 'mutation',
    'node', 'population', 'site'
]

class TestMetadataSchemas:

    def round_trip_schema(self, table):
        a = table.metadata_schema
        new_table = table.copy()
        new_table.assert_equals(table)
        b = new_table.metadata_schema
        assert a == b
        assert a.schema == b.schema
        new_table.clear()
        new_table.set_columns(**table.asdict())
        new_table.assert_equals(table)
        b = new_table.metadata_schema
        assert a == b
        assert a.schema == b.schema

    def copy_schema(self, table):
        new_table = table.__class__()
        new_table.schema = table.schema
        assert new_table.schema == table.schema
        assert new_table.schema.schema == table.schema.schema

    @pytest.mark.parametrize('recipe, table_name', indirect=['recipe'], argvalues=[
        ('test_____pop_names_pX.slim', x) for x in table_names
    ])
    def test_slim_canonical_json(self, recipe, table_name):
        # Here we want to test that whatever tskit does to the schema
        # when copying tables doesn't change the underlying .schema;
        # I can't figure out a good way to actually do that besides
        # making a copy of the tables.
        assert len(recipe["results"]) == 1
        ts = recipe["results"][0].get_normal_ts()
        t = ts.tables.table_name_map[table_name]
        tc = t.copy()
        # this should be true since it's been canonicalized
        # to produce the repr() which gets compared:
        assert t.metadata_schema == tc.metadata_schema
        if t.metadata_schema.schema != tc.metadata_schema.schema:
            # print out JSON that should give us a correct schema
            # so we can put it in the SLiM code
            j = tskit.canonical_json(tc.metadata_schema.schema)
            # this should succeed because json.loads returns an unordered
            # dictionary, and the two differ only on ordering
            assert json.loads(j) == t.metadata_schema.schema
            # put back in the placeholder
            if table_name == "nodes":
                u = '"length":1'
                ix = j.find(u)
                j = j[:ix] + '"length":"%d"' + j[(ix+len(u)):]
            print(f"Schema for {table_name} does not match. "
                  "Use this (equivalent) JSON.")
            print(".................... begin")
            print(j)
            print("...................... end")
        assert t.metadata_schema.schema == tc.metadata_schema.schema

    @pytest.mark.parametrize('recipe, table_name', indirect=['recipe'], argvalues=[
        ('test_____pop_names_pX.slim', x) for x in table_names
    ])
    def test_slim_round_trip(self, recipe, table_name):
        assert len(recipe["results"]) == 1
        ts = recipe["results"][0].get_normal_ts()
        self.round_trip_schema(ts.tables.table_name_map[table_name])

    @pytest.mark.parametrize('table_name', argvalues=table_names)
    def test_annotate_round_trip(self, table_name):
        ts = msprime.sim_ancestry(1, sequence_length=10, random_seed=123)
        ts = msprime.sim_mutations(ts, rate=1, random_seed=123)
        assert ts.num_mutations > 0
        t = ts.dump_tables()
        pyslim.annotate_tables(t, model_type='WF', tick=1)
        self.round_trip_schema(t.table_name_map[table_name])

    @pytest.mark.parametrize('table_name', argvalues=table_namex)
    def test_json_round_trip(self, table_name):
        # pyslim schema -> json -> schema
        orig = pyslim.slim_metadata_schemas[table_name]
        j = tskit.canonical_json(orig.schema)
        new = tskit.MetadataSchema(json.loads(j))
        assert new == orig
        assert new.schema == orig.schema



