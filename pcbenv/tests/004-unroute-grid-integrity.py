
import unittest
import pcbenv.tests.args as args
import pcbenv
import numpy as np

from importlib_resources import files

def grid_ko_route_vias(s):
    return np.bitwise_and(np.flip(s[0], axis=0), 1 << 7).astype(bool).astype(int)

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2", {'UserInterface': {'VisibleElements': ['!RatsNest','GridPoints']}})
        self.dsn_dir = files('pcbenv.data').joinpath('boards').joinpath('PCBBenchmarks-master')

    def test0(self):
        """
        Route and unroute in steps of single grid cells first with A-star then with segments.
        Check the grid returns to the same state after unrouting.
        """
        env = self.env
        rv = env.set_task({ 'pdes': str(self.dsn_dir.joinpath('bm1').joinpath('bm1.unrouted.dsn')),
                            'no_polygons': True,
                            'resolution_nm': 200000,
                            'state_representation': { 'default': 'grid', 'grid_box': ((258,178,0),(278,197,0)) } })
        self.assertTrue(rv)
        if args.RENDER:
            env.render("human")
        O = env.get_state({'board': 1})['board']['layout_area']
        XY = [(O[0]+263.5, O[1]+192.5, 0),
              (O[0]+270.5, O[1]+192.5, 0),
              (O[0]+271.5, O[1]+192.5, 0), # @0 and @1
              (O[0]+272.5, O[1]+192.5, 0), # @1 and @0
              (O[0]+273.5, O[1]+192.5, 0)] # @2

        for v in XY[:2]:
            env.step(('astar_to', (('ADC13',0),None,v)))

        S234 = []
        for v in XY[2:]:
            s, R, ok, T, rr = env.step(('astar_to', (('ADC13',0),None,v)))
            S234.append(grid_ko_route_vias(s))
        print("After step 2:\n", S234[0])

        S321 = []
        for _ in range(3):
            s, R, ok, T = env.step(('unroute_segment', (('ADC13',0),None)))
            S321.append(grid_ko_route_vias(s))
        print("After unrouting to step 2:\n", S321[1])

        env.step(('unroute_segment', (('ADC13',0),None)))
        s0, R, ok, T = env.step(('unroute_segment', (('ADC13',0),None)))
        s0 = grid_ko_route_vias(s0)
        print("After unrouting everything:\n", s0)

        for v in XY[:2]: # 2 STEPS
            env.step(('astar_to', (('ADC13',0),None,v)))

        S234_2 = []
        for v in XY[2:]:
            s, R, ok, T, rr = env.step(('segment_to', (('ADC13',0),None,v)))
            S234_2.append(grid_ko_route_vias(s))
        print("After routing to step 2 again:\n", S234_2[0])

        S321_2 = [] # STEPS 3,2,1
        for _ in range(3):
            s, R, ok, T = env.step(('unroute_segment', (('ADC13',0),None)))
            S321_2.append(grid_ko_route_vias(s))
        print("After unrouting to step 2 again:\n", S321_2[1])

        self.assertTrue(np.all(s0 == 0))
        self.assertTrue((S234[0] == S321[1]).all())
        self.assertTrue((S234[1] == S321[0]).all())
        self.assertTrue((S234_2[0] == S321_2[1]).all())
        self.assertTrue((S234_2[1] == S321_2[0]).all())
        args.stay_open()

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
