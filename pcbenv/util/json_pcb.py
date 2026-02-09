import json
import numpy as np

from pcbenv.util.math.vec2 import Vec2


class Bbox2:
    def __init__(self, x0, y0=None, x1=None, y1=None):
        if isinstance(x0, (list,tuple)):
            self.data = x0
        else:
            self.data = (x0,y0,x1,y1)

    def __getitem__(self, i):
        return self.data[i]

    def add(self, bbox):
        return Bbox2(min(self.x0, bbox.x0),
                     min(self.y0, bbox.y0),
                     max(self.x1, bbox.x1),
                     max(self.y1, bbox.y1))

    @property
    def x0(self):
        return self.data[0]
    @property
    def y0(self):
        return self.data[1]
    @property
    def x1(self):
        return self.data[2]
    @property
    def y1(self):
        return self.data[3]

    @property
    def w(self):
        return self.x1 - self.x0
    @property
    def h(self):
        return self.y1 - self.y0


class Shape:
    SHAPE_TO_ENUM = {
        'rect_iso': 0,
        'circle': 1,
        'polygon': 2,
        'wide_segment': 3
    }
    def __init__(self, data):
        self.data = data

    @property
    def type_str(self):
        return self.data[0]

    @property
    def type_enum(self):
        return self.SHAPE_TO_ENUM[self.type_str]

    def bbox(self):
        data = self.data
        if self.type_str == 'rect_iso':
            return Bbox2(data[1], data[2], data[3], data[4])
        if self.type_str == 'circle':
            r, x, y = data[1], data[2], data[3]
            return Bbox2(x - r, y - r, x + r, y + r)
        if self.type_str == 'wide_segment':
            wh = data[6] * 0.5
            x0 = min(data[1], data[3])
            y0 = min(data[2], data[4])
            x1 = max(data[1], data[3])
            y1 = max(data[2], data[4])
            dw = Vec2(x1-x0, y1-y0).rotated_90ccw().rescaled(wh).abs()
            return Bbox2(x0 - dw.x, y0 - dw.y, x1 + dw.x, y1 + dw.y)
        if self.type_str == 'polygon':
            xmin, ymin = float('inf'), float('inf')
            xmax, ymax = -xmin, -ymin
            for v in data[1]:
                xmin, xmax = min(xmin, v[0]), max(xmax, v[0])
                ymin, ymax = min(ymin, v[1]), max(ymax, v[1])
            return Bbox2(xmin, ymin, xmax, ymax)
        raise Exception("unknown shape: " + str(data[0]))


class Segment2:
    def __init__(self, A: Vec2, B: Vec2):
        self.A = A
        self.B = B
        self.v = B - A
        self.l2 = self.v.len_squared

    def distance(self, P: Vec2):
        t = (P - self.A).dot(self.v) / self.l2
        t = max(0.0, min(1.0, t))
        Q = self.A + self.v * t
        return (P - Q).len

    def mean_distance_p2s(self, s2, n=32):
        d = 0.0
        for i in range(n):
            t = i / (n - 1)
            P = self.A + self.v * t
            d += s2.distance(P)
        return d / n


class Connection:
    def __init__(self, data):
        self.data = data
        self.src = Vec2(data['src'])
        self.dst = Vec2(data['dst'])

    @property
    def distance_euclidean(self):
        return (self.dst - self.src).len
    @property
    def distance_45(self):
        return (self.dst - self.src).len45

    @property
    def segment(self):
        return Segment2(self.src, self.dst)

    @property
    def conn_id(self):
        s = self.data.get('src_pin', self.data['src'])
        d = self.data.get('dst_pin', self.data['dst'])
        return (s,d)
    @property
    def name(self):
        return str(self.conn_id)

    @property
    def net(self):
        return self.data['net']

    def bbox(self, track=True):
        box = Bbox2(min(self.src.x, self.dst.x),
                    min(self.src.y, self.dst.y),
                    max(self.src.x, self.dst.x),
                    max(self.src.y, self.dst.y))
        if not track:
            return box
        for T in self.data['tracks']:
            for s in t['segments']:
                box = box.add(Bbox2(min(s[0], s[2]),
                                    min(s[1], s[3]),
                                    max(s[0], s[2]),
                                    max(s[1], s[3])))
        return box

    def linkage_p2s(self, conn):
        s1 = self.segment
        s2 = conn.segment
        d1 = s1.mean_distance_p2s(s2)
        d2 = s2.mean_distance_p2s(s1)
        return (d1 + d2) * 0.5


class Pin:
    def __init__(self, data):
        self.data = data
        self.shape = Shape(data['shape'])

    def bbox(self):
        return self.shape.bbox()

    def min_extent(self):
        bbox = self.bbox()
        return min(bbox.w, bbox.h)

    def state_representation(self, nets):
        s = self.shape.type_enum
        n = self.data['net']
        c = nets[n]['clearance'] if n else 0
        d = nets[n]['via_diameter'] if n else 0
        n = nets[n]['id'] if n else -1
        Z = self.data['z']
        Z = [Z] if isinstance(Z, int) else range(Z[0], Z[1]+1)
        bbox = self.bbox()
        return [(bbox[0],
                 bbox[1],
                 bbox[2],
                 bbox[3],
                 z,
                 s,
                 n,
                 d,
                 c) for z in Z]
                

class JsonPCB:
    def __init__(self, data=None, task=None, path=None):
        self.data = task['json'] if task else data
        self.task = task
        if self.data is None:
            with path.open('r') as f:
                self.data = json.load(f)
        elif isinstance(self.data, str):
            self.data = json.loads(self.data)

    @property
    def filename(self):
        return self.data['file'][self.data['file'].rfind('/')+1:]

    @property
    def grid_size(self):
        return self.data['grid_size']

    @property
    def layout_area_bbox(self):
        return Bbox2(self.data['layout_area'])

    @property
    def num_layers(self):
        return len(self.data['layers'])
    @staticmethod
    def _is_signal_layer(L):
        return ('signal' in L['type']) or ('any' in L['type'])
    def signal_layers(self):
        return [L for L in self.data['layers'] if self._is_signal_layer(L)]
    @property
    def num_signal_layers(self):
        return len(self.signal_layers())

    @property
    def num_nets(self):
        return len(self.data['nets'])
    @staticmethod
    def _is_signal_net(net):
        return ('signal' in net['type']) or ('any' in net['type'])
    def signal_nets(self):
        return { name: net for name, net in self.data['nets'].items() if self._is_signal_net(net) }
    @property
    def num_signal_nets(self):
        return len(self.signal_nets())

    @property
    def num_connections(self):
        return sum(len(net['connections']) for net in self.data['nets'].values())
    @property
    def num_signal_connections(self):
        return sum(len(net['connections']) for net in self.signal_nets().values())

    def get_min_trace_width(self):
        w = float('inf')
        for net in self.data['nets'].values():
            w = min(w, net['track_width'])
            for x in net['connections']:
                for t in x['tracks']:
                    for s in t['segments']:
                        w = min(w, s[5])
        return w

    def get_min_pin_extent(self):
        return min(Pin(P).min_extent() for P in self.collect_pins())

    def _unrouted_connections(self):
        cons = []
        for net in self.data['nets'].values():
            for x in net['connections']:
                if len(x.get('tracks', [])) == 0:
                    cons.append(x)
        return cons
    def collect_unrouted_connections(self):
        return [Connection(x) for x in self._unrouted_connections()]

    def len_baseline(self):
        return sum([x['min_len'] for x in self._unrouted_connections()])

    def collect_connections(self, items={'nets': '*'}):
        nets = items.get('nets', [])
        nets = set(nets) if (nets != '*') else set(self.data['nets'].keys())

        pins = set(items.get('pins', []))
        for C in items.get('coms', []):
            for P in self.data['components'][C]['pins']:
                pins.add(C + '-' + P)

        cons = []
        for name, N in self.data['nets'].items():
            if name in nets:
                cons += [Connection(x) for x in N['connections']]
            elif len(pins):
                for x in N['connections']:
                    if x['src_pin'] in pins or x['dst_pin'] in pins:
                        cons.append(Connection(x))
        return cons

    def collect_tracks(self):
        T = []
        for N in self.data['nets'].values():
            for X in N['connections']:
                for track in X['tracks']:
                    track['net_id'] = N['id']
                T += X['tracks']
        return T

    def collect_vias(self):
        vias = []
        for N in self.data['nets'].values():
            for X in N['connections']:
                for T in X['tracks']:
                    vias.append(T['vias'])
        return vias

    def collect_pins(self):
        pins = []
        for C in self.data['components'].values():
            for P in C['pins'].values():
                pins.append(P)
        return pins

    def total_track_length(self):
        return sum(T['length'] for T in self.collect_tracks())
