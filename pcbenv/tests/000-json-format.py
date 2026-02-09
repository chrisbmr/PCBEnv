
import copy
import deepdiff
import json
import os
import time
import unittest
import pcbenv.tests.args as args
import pcbenv

# A random board in JSON format.
BOARD0 = {
    "name": "Test Board 0",
    "size": (100,100),
    "layers": 1,
    "unit": (1,-6), # 1 micrometer or 1e-6 meters
    "resolution": (1,-6),
    "pin_positions": "relative",
    "components": {
        "C0": {
            "pos": (51,50),
            "z": 0,
            "shape": ['rect_iso',15,15],
            "pins": {
                "T0": { "pos": (0,0), "shape": ['rect_iso',15,15] }
            }
        },
        "C1": {
            "pos": (51,80),
            "z": 0,
            "shape": ['rect_iso',10,10],
            "pins": {
                "T0": { "pos": (0,0), "shape": ['circle',5] }
            }
        },
        "C2": {
            "pos": (77,30),
            "z": 0,
            "shape": ['rect_iso',10,10],
            "pins": {
                "T0": { "pos": (0,0), "shape": ['circle',5] }
            }
        }
    },
    "nets": {
        "N0": { "pins": ["C0-T0","C1-T0","C2-T0"], "track_width": 5, "connections": [{"src_pin": "C0-T0", "dst_pin": "C1-T0"}] }
    }
}

# JSON string we expect to receive from env.get_state({'board': 3})['board']:
BOARD0_EXPECTED = """{
  "name": "Test Board 0",
  "file": "",
  "resolution_nm": 1000.0,
  "unit_nm": 1000.0,
  "grid_size": [
    100,
    100,
    1
  ],
  "layout_area": [
    0.0,
    0.0,
    100.0,
    100.0
  ],
  "layers": [
    {
      "id": 0,
      "type": "signal"
    }
  ],
  "nets": {
    "N0": {
      "type": "signal",
      "track_width": 5.0,
      "via_diameter": 5.0,
      "clearance": 5.0,
      "pins": [
        "C0-T0",
        "C1-T0",
        "C2-T0"
      ],
      "connections": [
        {
          "net": "N0",
          "src": [
            58.5,
            57.5,
            0
          ],
          "dst": [
            56.5,
            85.5,
            0
          ],
          "src_pin": "C0-T0",
          "dst_pin": "C1-T0",
          "clearance": 5.0
        },
        {
          "net": "N0",
          "src": [
            58.5,
            57.5,
            0
          ],
          "dst": [
            82.5,
            35.5,
            0
          ],
          "src_pin": "C0-T0",
          "dst_pin": "C2-T0",
          "clearance": 5.0
        }
      ]
    }
  },
  "components": {
    "C0": {
      "center": [
        58.5,
        57.5
      ],
      "z": 0,
      "shape": [
        "rect_iso",
        51.0,
        50.0,
        66.0,
        65.0
      ],
      "clearance": 0.0,
      "can_route": false,
      "via_ok": true,
      "pins": {
        "T0": {
          "center": [
            58.5,
            57.5
          ],
          "z": 0,
          "shape": [
            "rect_iso",
            51.0,
            50.0,
            66.0,
            65.0
          ],
          "clearance": 0.0,
          "net": "N0",
          "connects_to": [
            "C1-T0",
            "C2-T0"
          ]
        }
      }
    },
    "C1": {
      "center": [
        56.0,
        85.0
      ],
      "z": 0,
      "shape": [
        "rect_iso",
        51.0,
        80.0,
        61.0,
        90.0
      ],
      "clearance": 0.0,
      "can_route": false,
      "via_ok": true,
      "pins": {
        "T0": {
          "center": [
            56.0,
            85.0
          ],
          "z": 0,
          "shape": [
            "circle",
            5.0,
            56.0,
            85.0
          ],
          "clearance": 0.0,
          "net": "N0",
          "connects_to": [
            "C0-T0"
          ]
        }
      }
    },
    "C2": {
      "center": [
        82.0,
        35.0
      ],
      "z": 0,
      "shape": [
        "rect_iso",
        77.0,
        30.0,
        87.0,
        40.0
      ],
      "clearance": 0.0,
      "can_route": false,
      "via_ok": true,
      "pins": {
        "T0": {
          "center": [
            82.0,
            35.0
          ],
          "z": 0,
          "shape": [
            "circle",
            5.0,
            82.0,
            35.0
          ],
          "clearance": 0.0,
          "net": "N0",
          "connects_to": [
            "C0-T0"
          ]
        }
      }
    }
  }
}"""

BOARD1 = copy.deepcopy(BOARD0)
BOARD1['layers'] = 2

class TestCase(unittest.TestCase):
    def setUp(self):
        self.env = pcbenv.make("pcb-v2", conf={"UserInterface": { "NanoMetersPerPixel": int(100000/1280) }})

    def sort_pins(self, J):
        for net in J['nets'].values():
            net['pins'] = sorted(net['pins'])
        for C in J['components'].values():
            for P in C['pins'].values():
                P['connects_to'] = sorted(P['connects_to'])

    def json_diff(self, J1, J2):
        rv = 0
        J1 = json.loads(J1 if isinstance(J1, str) else json.dumps(J1))
        J2 = json.loads(J2 if isinstance(J2, str) else json.dumps(J2))
        DJ = deepdiff.DeepDiff(J1,J2)
        print("Dictionary differences:", DJ)
        return len(DJ)

    def test1(self):
        """Test that routing, retrieving, and setting a track works and returns the same track back."""
        env = self.env
        rv = env.set_task({ "json": json.dumps(BOARD1), "resolution_nm": "auto" })
        self.assertTrue(rv)
        P = [(55.5,85.5,0),(90.5,85.5,0),(90.5,85.5,1),(90.5,50.5,0)]
        env.step(('astar_to', (('N0',0), P[0], P[1])))
        env.step(('segment_to', (('N0',0), P[1], P[2])))
        env.step(('astar_to', (('N0',0), P[2], P[3])))
        track_r = env.get_state({'board': {'depth':4,'numpy':False}})['board']['nets']['N0']['connections'][0]['tracks'][0]
        print("Routed track for N0/0:\n", track_r)
        env.step(('unroute', ('N0',0)))
        env.step(('set_track', (('N0',0),track_r)))
        track_s = env.get_state({'board': {'depth':4,'numpy':False}})['board']['nets']['N0']['connections'][0]['tracks'][0]
        print("Set track for N0/0:\n", track_s)
        self.assertEqual(self.json_diff(track_r, track_s), 0)

    def test0(self):
        """Test that getting the board representation with get_state() and then loading that as a new board does not change anything."""
        rv = self.env.set_task({ "json": json.dumps(BOARD0), "resolution_nm": "design" })
        self.assertTrue(rv)
        J1 = self.env.get_state({'board': 3})["board"]
        self.sort_pins(J1)

        rv = self.env.set_task({ "json": json.dumps(J1) })
        self.assertTrue(rv)
        J2 = self.env.get_state({'board': 3})["board"]
        self.sort_pins(J2)

        self.assertEqual(self.json_diff(BOARD0_EXPECTED, J1), 0)
        self.assertEqual(self.json_diff(J1, J2), 0)

    def tearDown(self):
        self.env.close()

if __name__ == "__main__":
    unittest.main()
