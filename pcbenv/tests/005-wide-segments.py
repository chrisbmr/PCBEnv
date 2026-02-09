
import numpy as np
import time
import unittest
import pcbenv.tests.args as args
import pcbenv

from importlib_resources import files

def grid_extract_bit(s, mask):
    return np.bitwise_and(np.flip(s[0], axis=0), mask).astype(bool).astype(int)

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2", {'UserInterface': {'VisibleElements': ['!RatsNest','GridPoints']}})
        self.dsn_dir = files('pcbenv.data').joinpath('boards').joinpath('PCBBenchmarks-master')

    def test0(self):
        env = self.env
        rv = env.set_task({ "pdes": str(self.dsn_dir.joinpath('bm1').joinpath('bm1.routed.kicad_pcb')), 'load_tracks': False, 'resolution_nm': 175000, 'no_polygons': True })
        env.step(('astar',('PE5',0)))
        grid = env.get_state({'grid': ((312,278,0),(318,283,0))})['grid']
        grid = grid_extract_bit(grid, 0x30)
        grid_expected = np.array([[1, 1, 1, 1, 1, 1, 1],
                                  [0, 1, 1, 1, 1, 1, 1],
                                  [0, 1, 1, 1, 1, 1, 1],
                                  [0, 0, 1, 1, 1, 1, 1],
                                  [0, 0, 0, 0, 1, 1, 1],
                                  [0, 0, 0, 0, 0, 0, 0]])
        self.assertTrue((grid == grid_expected).all())

    def test1(self):
        env = self.env
        rv = env.set_task({ "pdes": str(self.dsn_dir.joinpath('bm1').joinpath('bm1.routed.kicad_pcb')), 'load_tracks': True, 'resolution_nm': 205000 })
        data = env.get_state({'board': 2})['board']
        for name in data['nets']:
            net = data['nets'][name]
            for i in range(len(net['connections'])):
                if name != "+5V" or i != 2:
                    env.step(('unroute', (name,i)))
        env.step(('astar',('PE5',0)))
        grid = env.get_state({'grid': ((42,66,0),(45,70,0))})['grid']
        grid = grid_extract_bit(grid, 0x40)
        print("Expecting all ones for segment KO count except bottom right:")
        print(grid)
        grid[4,3] = 1 - grid[4,3]
        self.assertTrue((grid == 1).all())

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
