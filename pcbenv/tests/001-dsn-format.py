
import unittest
import pcbenv.tests.args as args
import pcbenv

from importlib_resources import files

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2")
        self.dsn_dir = files('pcbenv.data').joinpath('boards').joinpath('PCBBenchmarks-master')

    def test0(self):
        rv = self.env.set_task({ "pdes": str(self.dsn_dir.joinpath('bm1').joinpath('bm1.unrouted.dsn')), "no_polygons": True })
        self.assertTrue(rv)

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
