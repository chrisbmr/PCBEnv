
import numpy as np
import math
import time
import unittest
from importlib_resources import files
import pcbenv
import pcbenv.tests.args as args
from pcbenv.util.json_pcb import JsonPCB

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2", {'UserInterface': {'VisibleElements': ['!RatsNest','!GridPoints']}})
        self.dsn_dir = files('pcbenv.data').joinpath('boards').joinpath('PCBBenchmarks-master')
        self.env.set_task({ "pdes": str(self.dsn_dir.joinpath('bm1').joinpath('bm1.routed.kicad_pcb')), 'load_tracks': False, 'resolution_nm': 200000, 'no_polygons': True, 'state_representation': { 'default': 'track' }, 'fixed_track_params': True, 'min_via_diam': 400, 'min_trace_width': 1, 'min_clearance': 100 })

    def test0_RRR(self):
        """
        Test that RRR returns the expected result given a parameter and reroute policy.
        """
        class Policy:
            """
            RRR policy that just reroutes everything in the order from shortest to longest connection.
            """
            def __init__(self, cons):
                self.cons = cons
            def P(self, states):
                """
                @param states This is just the connections' names.
                """
                L = [self.cons[str(x)].distance_euclidean for x in states]
                I = np.argsort(L)
                return np.array(I, dtype=np.float32)

        env = self.env

        # Collect connection information.
        jpcb = JsonPCB(data=env.get_state({'board': 3})['board'])
        cons = jpcb.collect_connections()
        cons = { x.name: x for x in cons }

        env.render("human")
        env.set_agent(('rrr', {
            'history_cost_increment': 1/32,
            'max_iterations': 64,
            'max_iterations_stagnant': 8,
            'tidy_iterations': 2,
            'randomize_order': False, # we use a policy to determine the order
            'reward': {
                'function': 'track_length',
                'per_unrouted': -5,
                'per_via': 0,
                'scale_length': 'd45',
                'scale_global': ''
            },
            'astar_costs': {
                'wd': 1, # 1 = XY routing is off
                'dir': 'xy'
            },
            'py_interface': Policy(cons)
        }))
        env.run_agent({'nets': ['ADC'+str(i) for i in range(16)] + ['PL'+str(i) for i in range(8)] + ['SCL', 'SDA', 'RXD1', 'TXD1', 'PD7', 'N$2']})
        rv = env.get_state('stats')
        self.assertTrue(rv['TimeLine'][2]['Success'])

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
