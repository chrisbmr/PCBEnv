
import numpy as np
import time
import unittest
import pcbenv.tests.args as args
import pcbenv

from importlib_resources import files

NET_REF  = ("PH4",0)
NET1_REF = NET_REF
NET2_REF = ("PH3",0)
NET3_REF = ("TXD2",0)

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2", {'UserInterface': {'VisibleElements': ['!RatsNest','!GridPoints']}})
        self.dsn_dir = files('pcbenv.data').joinpath('boards').joinpath('PCBBenchmarks-master')
        self.env.set_task({ "pdes": str(self.dsn_dir.joinpath('bm1').joinpath('bm1.routed.kicad_pcb')), 'load_tracks': False, 'resolution_nm': 200000, 'no_polygons': True, 'state_representation': { 'default': 'track' } })

    def test0_LayerMask(self):
        """
        Check that masking layer 0 results in the track using layer 1.
        """
        env = self.env
        print("Setting layer mask for PH4")
        s = env.step(("set_layer_mask", ("PH4", 0x2)))[0]
        # No track used so expect state to be None.
        self.assertIsNone(s)
        s = env.step(("astar", NET_REF))[0]
        self.assertEqual(int(s[0]['path'][2][2]), 1)

    def test1_RouteGuard(self):
        """
        Test that setting a route guard path as a horizontal line blocking the shortest path makes the track longer,
        and that removing the route guard returns the original length.
        """
        env = self.env
        l1 = env.step(("astar", NET_REF))[0][0]['length']
        env.step(("unroute", NET_REF))
        env.step(("set_layer_mask", ("PH4", 0x1)))
        env.step(("set_guard",[(650,-450,0),(800,-450,0)]))
        l2 = env.step(("astar", NET_REF))[0][0]['length']
        env.step(("unroute", NET_REF))
        env.step(("set_guard",None))
        l3 = env.step(("astar", NET_REF))[0][0]['length']
        self.assertEqual(l1,l3)
        self.assertGreater(l2,l1)

    def test1b_RouteGuard_2path(self):
        env = self.env
        env.step(('set_guard', np.array([(650,-450,0),(800,-450,0),(0,0,-1),(650,-430,0),(800,-430,0)], dtype=np.float32)))
        env.step(("astar", NET_REF))
        s = env.get_state({'grid': ((220,208,0),(220,228,0))})['grid']
        self.assertTrue(np.array_equal(s[0,:,0], [2048]+[0]*19+[2048]))

    @staticmethod
    def convert_track(segs):
        return [(s[0],s[1],s[2],s[3],int(s[4]),s[5]*2) for s in segs.tolist()]

    def test2_SetTrack(self):
        env = self.env
        s1 = env.step(("astar", NET_REF))[0][0]['segments']
        s1 = self.convert_track(s1)
        env.step(("unroute", NET_REF))
        s2 = env.step(("set_track", (NET_REF, {'segments': s1})))[0][0]['segments']
        s2 = self.convert_track(s2)
        s1_array = np.array(s1)
        with self.assertRaises(Exception) as context:
            s2 = env.step(("set_track", (NET_REF, {'segments': s1_array})))[0][0]['segments']
        s1_array = s1_array.astype(np.float32)
        s2_array = env.step(("set_track", (NET_REF, {'segments': s1_array})))[0][0]['segments']
        self.assertEqual(len(s1), len(s2))
        for i in range(len(s1)):
            print(s1[i])
            print(s2[i])
            self.assertEqual(s1[i], (*s2[i][:5], s2[i][5]*0.5))
        s2 = np.array(s2, np.float32)
        self.assertTrue(s2.shape == s2_array.shape)
        self.assertTrue(np.max(np.abs(s2 - s2_array)) < 1e-5)

    def test3_CostMap(self):
        """
        Use a cost map such that routing succeeds.
        """
        env = self.env
        s, R, ok1, T, D = env.step(("astar", NET1_REF))
        s, R, ok2, T, D = env.step(("astar", NET2_REF))
        s, R, ok3, T, D = env.step(("astar", NET3_REF))
        self.assertTrue(ok1)
        self.assertTrue(ok2)
        self.assertFalse(ok3)
        env.step(("unroute", NET2_REF))
        s, R, ok, T, D = env.step(("astar", NET3_REF))
        self.assertTrue(ok)
        env.step(("unroute", NET3_REF))
        boxBL = (204,135,0)
        boxTR = (225,150,0)
        env.step(("set_costs", (32.0, boxBL, boxTR)))
        s, R, ok2, T, D = env.step(("astar", NET2_REF))
        s, R, ok3, T, D = env.step(("astar", NET3_REF))
        self.assertTrue(ok2)
        self.assertTrue(ok3)

    def test0_AstarCosts(self):
        env = self.env
        xref = ('PA3',0)
        costs0 = { 'wd': 3, 'wl': 1, 'dir': '' }
        costs1 = { 'wd': 3, 'wl': 1, 'dir': 'xy' }
        s = env.step(('astar', (xref, costs0)))
        self.assertEqual(len(s[0][0]['vias']), 0)
        s = env.step(('astar', (xref, costs1)))
        self.assertEqual(len(s[0][0]['vias']), 1)

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
