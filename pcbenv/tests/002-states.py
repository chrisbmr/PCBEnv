
import numpy as np
import math
import time
import unittest
import pcbenv.tests.args as args
import pcbenv
import pcbenv.util.plot.image as image

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

    def test0_DRCCheck(self):
        """
        Check that routing 2 connections that cross if DRV cost is 1 indentifies the issue in the DRC check.
        """
        env = self.env
        drc = env.get_state({'clearance_check': {'nets': ['PB6']}})['clearance_check']
        self.assertTrue(len(drc) == 0)
        costs = { 'drv': 1 }
        env.step(('astar', (('PB6',0), {})))
        env.step(('astar', (('AREF',0), costs)))
        env.step(('astar', (('AREF',1), costs)))
        drc = env.get_state({'clearance_check': {'nets': ['PB6']}})['clearance_check']
        self.assertTrue(len(drc) == 1)
        self.assertAlmostEqual(drc[0]['x'][0], 656.0, places=1)
        self.assertAlmostEqual(drc[0]['x'][1], -442.86, places=1)

    def test1_Endpoints(self):
        """
        Test that all reported endpoints correspond to exactly one connection as reported by 'board' state.
        """
        env = self.env
        s = env.get_state({'ends': None})['ends']
        B = env.get_state({'board': 3})['board']
        X = []
        self.assertEqual(s.shape, (195,6))
        for name, net in B['nets'].items():
            for conn in net['connections']:
                X.append(conn['src'] + conn['dst'])
        X = np.array(X, dtype=np.float32)
        for x in X:
            self.assertEqual(np.sum(np.all(s == x, axis=1)), 1)

    def test2_Features(self):
        order = ['Routed', 'TrackLen', 'TrackLenRatio', 'NumVias', 'FractionX', 'Fraction45', 'FractionY', 'ExcessFractionX', 'ExcessFractionY', 'RMSD', 'MaxDeviation', 'FractionOutsideAABB', 'IntersectionsRat', 'IntersectionsTrack', 'Winding', 'Turning', 'PinSpace1', 'PinSpace2', 'LayerUtilization', 'MinTrackLen', 'DistanceFractionX']
        env = self.env
        env.step(('astar', NET_REF))
        sd = env.get_state({'features': {'as_dict': True}})['features']
        sv = env.get_state({'features': None})['features']
        F = [x for x in sd.values() if x.get('Net') == NET_REF[0]][0]
        self.assertTrue(F['Routed'])
        self.assertGreater(F['TrackLenRatio'], 1.02)
        self.assertEqual(int(F['TrackLen']), 128)
        self.assertAlmostEqual(F['Turning'], math.pi, places=4)
        self.assertAlmostEqual(F['Winding'], -math.pi/2, places=4)
        self.assertAlmostEqual(F['DistanceFractionX'], 0.104, places=3)
        self.assertAlmostEqual(F['RMSD'], 4.5, places=1)
        self.assertAlmostEqual(F['ExcessFractionX'], 0, places=4)
        self.assertAlmostEqual(F['ExcessFractionY'], 0, places=4)
        self.assertEqual(F['IntersectionsTrack'], 0)
        self.assertEqual(F['IntersectionsRat'], 16)
        self.assertEqual(F['LayerUtilization'], 0.5)
        self.assertAlmostEqual(F['PinSpace1'], 0.829, places=3)
        self.assertAlmostEqual(F['PinSpace2'], 1.0, places=3)
        n = int(np.argwhere(sv[3:,0] == 1)) + 3
        Fv = np.array([F[k] for k in order], dtype=np.float32)
        self.assertTrue(np.array_equal(sv[n], Fv))

    def test3_ImageLike(self):
        env = self.env
        with self.assertRaises(Exception) as context:
            env.get_state({'image_grid': {'max_scale': 1, 'max_size': (64,64), 'min_scale': 0.5}})
        with self.assertRaises(Exception) as context:
            env.get_state({'image_draw': {'max_scale': 1, 'max_size': (64,64), 'min_scale': 0.5}})
        X0 = env.get_state({'image_grid': {'max_size': (1024,1024), 'max_scale': 0.2, 'min_scale': 0.1 }})['image_grid']
        X1 = env.get_state({'image_grid': {'size': (1024,1024), 'max_size': (0,0) }})['image_grid']
        X2 = env.get_state({'image_grid': {}})['image_grid']
        X3 = env.get_state({'image_grid': {'size': (256,256) }})['image_grid']
        X4 = env.get_state({'image_draw': {'size': (256,256), 'max_size': (0,0) }})['image_draw']
        X5 = env.get_state({'image_grid': {'max_size': (384,384), 'max_scale': 1.0, 'min_scale': 0.1 }})['image_grid']
        X6 = env.get_state({'image_grid': {'max_size': (384,384), 'max_scale': 0.8, 'min_scale': 0.1 }})['image_grid']
        XZ = X1[:,500:]
        self.assertTrue(np.array_equal(X1,X2))
        self.assertTrue(np.sum(X3 != X4)/np.prod(X3.shape) < 0.015)
        self.assertTrue(np.sum(X3 != X4)/np.sum(X3 != 0) < 0.29)
        self.assertTrue(np.all(XZ == 0))
        self.assertTrue(X0.shape[0] < 128)
        self.assertEqual(X5.shape, X6.shape)

    def test4_SelectItems(self):
        env = self.env
        with self.assertRaises(Exception) as context:
            env.get_state({'select': {'object': 'pin'}})
        with self.assertRaises(Exception) as context:
            env.get_state({'select': {'object': 'pin', 'box': 3}})
        sel1 = env.get_state({'select': {'object': 'pin', 'box': ((645,-500,0),(646,-470,0)) }})['select']
        sel2 = env.get_state({'select': {'object': 'pin', 'pos': (645,-500,0) }})['select']
        sel3 = env.get_state({'select': {'object': 'com', 'pos': (645,-500,0) }})['select']
        self.assertEqual(sorted(sel1), ['U8-1', 'U8-2', 'U8-3'])
        self.assertEqual(sorted(sel2), ['U8-3'])
        self.assertEqual(sorted(sel3), ['U8'])

    def test5a_Tracks(self):
        env = self.env
        CON1 = ('PB6',0)
        CON2 = ('PB5',0)
        T1 = env.step(('astar', CON1))[0][0]
        T2 = env.step(('astar', CON2))[0][0]
        env.step(('astar', CON2))
        T = env.get_state({'track': [CON1, CON2]})['track']
        self.assertEqual(len(T), 2)
        self.assertTrue(np.array_equal(T1['segments'], T[0][0]['segments']))
        self.assertTrue(np.array_equal(T2['segments'], T[1][0]['segments']))
        A1 = T1['segments'][0][0:2]
        E1 = T1['segments'][-1][2:4]
        A2 = np.array(T1['start'][:2], np.float32)
        E2 = np.array(T1['end'][:2], np.float32)
        self.assertTrue(np.array_equal(A1, A2))
        self.assertTrue(np.array_equal(E1, E2))

    def test5b_Raster(self):
        env = self.env
        CON1 = ('N$3',0)
        CON2 = ('PB6',0)
        T1 = env.step(('astar', CON1))[0][0]
        print("Rasterizing track", T1['segments'])
        s = env.get_state({'raster': [CON1,CON2]})['raster']
        self.assertEqual(len(s[1]), 0)
        I = np.argsort(s[0][0][:,0])
        X = s[0][0][I]
        self.assertEqual(min(X[:,0]), 386)
        self.assertEqual(max(X[:,0]), 397)
        self.assertEqual(min(X[:,1]), 173)
        self.assertEqual(max(X[:,1]), 185)
        self.assertEqual(len(X), 23)

    def test6_Metrics(self):
        env = self.env
        nets = list(env.get_state({'board': 2})['board']['nets'].keys())
        print(nets)
        m_all1 = env.get_state({'metric': 'ratsnest_crossings'})['metric']
        m_all2 = env.get_state({'metric': ('ratsnest_crossings', nets)})['metric']
        m_all3 = env.get_state({'metric': ('ratsnest_crossings', set(nets))})['metric']
        self.assertEqual(m_all1, 932)
        self.assertEqual(m_all1, m_all2)
        self.assertEqual(m_all2, m_all3)
        m_two = env.get_state({'metric': ('ratsnest_crossings', {'PB6', 'AREF', 'PB5'})})['metric']
        self.assertEqual(m_two, 2)

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
