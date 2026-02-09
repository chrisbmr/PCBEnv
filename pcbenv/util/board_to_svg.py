
import argparse
import colorsys
import copy
import json
from pathlib import Path
from reportlab.graphics import renderPDF
import sys
from svglib.svglib import svg2rlg
import svgwrite
import tempfile
import shapely

THEME = 'light'

BROKEN_STUFF = True

def HSV(h,s,v):
    r, g, b = colorsys.hsv_to_rgb(h / 360, s, v)
    return svgwrite.rgb(int(r * 100), int(g * 100), int(b * 100), '%')
def RGB(r,g,b):
    return svgwrite.rgb(r,g,b,'%')

THEME_LIGHT = {
    'bg': 'white',
    'rat': HSV(0,0,0.1),
    '_layer': [
        #HSV(  0, 0.7, 0.9), # bottom: red
        HSV(210, 1.0, 0.9), # top (blue)
        HSV(117, 0.8, 0.9), # green
        HSV( 50, 0.8, 0.9), # dark yellow
        HSV(278, 0.8, 0.9), # purple
        HSV(150, 0.8, 0.9), # mint
        HSV( 32, 1.0, 1.0), # light orange
        HSV( 80, 1.0, 1.0), # poison
        HSV(185, 1.0, 1.0), # cyan
        HSV(302, 1.0, 1.0), # magenta
        HSV(332, 1.0, 1.0), # pink
        HSV(  0, 0.0, 0.7), # light grey
        HSV( 25, 0.6, 0.7), # rust
        HSV( 60, 0.7, 0.8), # yellow
        HSV(207, 1.0, 1.0), # blue
        HSV(  0, 0.0, 1.0), # white
        HSV(260, 0.3, 0.8), # metal
        HSV(  0, 0.7, 0.9), # bottom: red
        #HSV(210, 1.0, 0.9), # top (blue)
    ],
    'cluster': [
        HSV(  0,1,1.0),
        HSV( 40,1,0.9),
        HSV(120,1,0.8),
        HSV(220,1,0.7),
        HSV(180,1,1.0),
        HSV( 60,1,0.9),
        HSV(280,1,0.8),
        HSV(330,1,0.7)
    ],
    'top': HSV(210, 0.0, 0.5),
    'bot': HSV(  0, 0.7, 1.0),
    'pin_d': [HSV(  0, 0.0, 0.5), HSV(  0, 0.0, 0.7)],
    'pin_c': [HSV(210, 1.0, 1.0), HSV(  0, 0.7, 1.0)],
    'layout': RGB(0,0,0),
    'hole': HSV(0, 0.0, 0.4),
    'outline': HSV(0,0,0)
}
THEME = THEME_LIGHT

COORD_BASE = (0, 0)
COORD_SIZE = (0, 0)
COORD_SCALE = 1

DOC_SIZE = ('100%', '100%')
DOC_BASE = (0, 0)

def is_PWR_or_GND(net, name):
    if ('power' in net['type']) or ('ground' in net['type']):
        return True
    return False

def set_coordinate_system(box):
    global COORD_BASE
    global COORD_SIZE
    global DOC_BASE
    global DOC_SIZE
    DOC_SIZE = ('100%', '100%')
    DOC_BASE = (0, 0)
    COORD_BASE = (box[0], box[1])
    COORD_SIZE = (box[2] - COORD_BASE[0], box[3] - COORD_BASE[1])
    if False:
        print("Coordinate space:")
        print(COORD_BASE)
        print(COORD_SIZE)
    if True:
        w, h = COORD_SIZE
        DOC_SIZE = (w, h)

def convert_len(s):
    return s / COORD_SCALE
def convert_coords(v):
    v = (convert_len(v[0] - COORD_BASE[0]) - DOC_BASE[0],
         convert_len(v[1] - COORD_BASE[1]) - DOC_BASE[1])
    return (v[0], DOC_SIZE[1] - v[1])

def layer_min(z):
    return z[0] if hasattr(z, '__len__') else z
def on_bottom(A):
    return layer_min(A['z']) > 0

def set_document_area(conn_ends):
    global DOC_BASE
    global DOC_SIZE
    x_min, y_min = conn_ends[0][0]
    x_max, y_max = x_min, y_min
    for ends in conn_ends:
        for v in ends[:2]:
            x_min = min(x_min, v[0])
            x_max = max(x_max, v[0])
            y_min = min(y_min, v[1])
            y_max = max(y_max, v[1])
    x_min, y_min = convert_coords((x_min, y_min))
    x_max, y_max = convert_coords((x_max, y_max))
    w = x_max - x_min
    h = y_max - y_min
    eXL = w * 0.2
    eXR = w * 0.1
    eYT = h * 0.09
    eYB = h * 0.12
    DOC_SIZE = (str(w + eXL + eXR), str(h + eYB + eYT))
    if BROKEN_STUFF:
        DOC_SIZE = (w + eXL + eXR, h + eYB + eYT)
    DOC_BASE = (x_min - eXL, y_min - eYB)

def load_box(box):
    TL = convert_coords((box[0], box[1]))
    BR = convert_coords((box[2], box[3]))
    BL = (TL[0], BR[1])
    return (BL, (BR[0] - BL[0], abs(TL[1] - BL[1])))
def box_center(box):
    BL, (W, H) = load_box(box)
    return (BL[0] + W * 0.4, BL[1] + H * 0.4)
def box_maxdim(box):
    BL, (W, H) = load_box(box)
    return max(W,H)
def write_box(svg, box, stroke, parm={'fill': 'none'}):
    box = load_box(box)
    return svg.rect(box[0], box[1], stroke=stroke, **parm)

def layout_area(svg, data):
    return write_box(svg, data['layout_area'], THEME['layout'], {'fill': THEME['bg']})

def write_circle(svg, center, data, parm):
    return svg.circle(center, convert_len(data[1]), **parm)
def write_isorect(svg, center, data, parm):
    w = convert_len(data[3] - data[1])
    h = convert_len(data[4] - data[2])
    a = 0 if 'angle' not in data else data['angle']
    if a == 90 or a == 270:
        w, h = h, w
    return svg.rect((center[0] - w/2, center[1] - h/2), (w, h), **parm)
def write_polygon(svg, center, data, parm):
    verts = [convert_coords(x) for x in data[1]]
    verts = list(filter(lambda v: max(abs(v[0]), abs(v[1])) < 10000, verts))
    return svg.polygon(verts, **parm)
def write_widesegment(svg, center, data, parm):
    x0 = convert_coords((data[1], data[2]))
    x1 = convert_coords((data[3], data[4]))
    w = convert_len(data[6])
    return svg.line(start=x0, end=x1, stroke_width=w, stroke=parm['fill'], stroke_opacity=parm['fill-opacity'], stroke_linecap='round')
write_shape_map = {
    'circle': write_circle,
    'rect_iso': write_isorect,
    'polygon': write_polygon,
    'wide_segment': write_widesegment
}
def write_shape(svg, center, shape, parm):
    return write_shape_map[shape[0]](svg, center, shape, parm)

def shape_maxdim(shape):
    if shape[0] == 'circle':
        return convert_len(shape[1])
    if shape[0] == 'rect_iso':
        return convert_len(max(shape[3] - shape[1], shape[4] - shape[2]))
    if shape[0] == 'polygon':
        bound = shapely.Polygon(shape[1]).bounds
        return convert_len(max(bound[1] - bound[0], bound[3] - bound[2]))
    raise Exception(f"maxdim: unknown shape {shape}")

def write_pin(svg, data):
    pos = convert_coords(data['center'])
    bot = 1 if on_bottom(data) else 0
    col = data['color'] if 'color' in data else THEME['pin_d'][bot]
    if ('shape' in data) and data['shape'] is not None:
        svg.add(write_shape(svg, pos, data['shape'], {'fill': col, 'fill-opacity': 0.9, 'stroke-width': 0.125, 'stroke': THEME['outline'] }))

def font_size_for_shape(shape):
    d = shape_maxdim(shape)
    h = DOC_SIZE[1]
    MIN_FONT_SIZE = h / 150
    MAX_FONT_SIZE = h / 100
    return max(MIN_FONT_SIZE, min(d/16, MAX_FONT_SIZE))

def write_component(svg, name, data, side):
    if on_bottom(data) != (side == 't'):
        return
    center = convert_coords(data['center'])
    side = THEME['bot'] if on_bottom(data) else THEME['top']
    area = write_shape(svg, center, data['shape'], { 'stroke': side, 'fill': 'none' })
    svg.add(area)
    if svg.mylabels:
        svg.mylabels.add(svg.text(name, center, text_anchor='middle', font_size=font_size_for_shape(data["shape"])))
    if 'pins' in data and data['pins']:
        for pin in data["pins"]:
            write_pin(svg, data["pins"][pin])

def write_ratsnest(svg, conns_ends):
    if not conns_ends:
        return
    for ends in conns_ends:
        c = THEME['rat'] if (ends[2] is None) else THEME['cluster'][ends[2] % len(THEME['cluster'])]
        w = 0.5 if (ends[2] is None) else 1
        v0 = convert_coords(ends[0])
        v1 = convert_coords(ends[1])
        svg.add(svg.line(v0, v1, stroke=c, **{'stroke-width': w}))

def start_segments_path(svg, v, col, width):
    path = svgwrite.path.Path("M " + str(v[0]) + ',' + str(v[1]), fill='none', stroke=col, stroke_width=width, **{'stroke-linecap': 'round'})
    svg.add(path)
    return path
def write_segments(svg, net, max_width=float('inf')):
    v1 = None
    l1 = None
    for conn in net['connections']:
        if not ('tracks' in conn):
            continue
        tracks = conn['tracks']
        for track in conn['tracks']:
            segs = track['segments'].tolist()
            if len(segs) == 0:
                continue
            z0 = -1
            w0 = -1
            v0 = convert_coords((segs[0][0], segs[0][1]))
            for s in segs:
                v1 = convert_coords((s[2], s[3]))
                z1 = int(s[4])
                w1 = convert_len(s[5])
                if z0 != z1 or w0 != w1:
                    if w1 <= max_width:
                        path = start_segments_path(svg, v0, THEME['layer'][z1], width=w1)
                if w1 <= max_width:
                    path.push("L " + str(v1[0]) + ',' + str(v1[1]))
                z0 = z1
                v0 = v1
                w0 = w1

def collect_endpoints(conns):
    ends = []
    for conn in conns:
        ends.append((conn['src'], conn['dst'], conn.get('cluster', None)))
    return ends

def mark_connected_pins(data, net):
    if net["pins"] is None:
        return
    for pin_name in net["pins"]:
        C, P = pin_name.split('-', 1)
        P = data['components'][C]['pins'][P]
        P["color"] = THEME['pin_c'][1 if on_bottom(P) else 0]
def collect_ends_and_mark_connections(data, conns):
    ends = []
    for conn in conns:
        C1, P1 = conn['src_pin'].split('-')
        C2, P2 = conn['dst_pin'].split('-')
        P1 = data["components"][C1]["pins"][P1]
        P2 = data["components"][C2]["pins"][P2]
        assert P1['center'] is not None
        assert P2['center'] is not None
        ends.append((P1['center'], P2['center'], conn.get('cluster', None)))
        P1['color'] = THEME['pin_c'][1 if on_bottom(P1) else 0]
        P2['color'] = THEME['pin_c'][1 if on_bottom(P2) else 0]
    return ends

def des2svg(data, connections, dest, draw_tracks=False, draw_PWR_GND=False, labels=False, flip_y=True):
    THEME['layer'] = copy.copy(THEME['_layer'])
    THEME['layer'][data['layers'][-1]['id']] = THEME['layer'][-1]

    set_coordinate_system(data["layout_area"])
    if connections:
        connection_ends = collect_ends_and_mark_connections(data, connections)
        if not BROKEN_STUFF:
            set_document_area(connection_ends)
    else:
        connection_ends = None
        for name in data["nets"]:
            mark_connected_pins(data, data["nets"][name])

    svg = svgwrite.Drawing(dest, profile='tiny', size=DOC_SIZE, viewBox=f"0 0 {DOC_SIZE[0]} {DOC_SIZE[1]}")
    svg.mylabels = svg.g(font_family="Sans") if labels else None
    if True:
        svg.add(layout_area(svg, data))
    if draw_tracks:
        for name, net in data["nets"].items():
            if not draw_PWR_GND and is_PWR_or_GND(net, name):
                continue
            write_segments(svg, net)
    for name in data["components"]:
        write_component(svg, name, data["components"][name], 't')
    write_ratsnest(svg, connection_ends)
    for name, net in data["nets"].items():
        if not draw_tracks and (connection_ends is None):
            if not draw_PWR_GND and is_PWR_or_GND(net, name):
                continue
            write_ratsnest(svg, collect_endpoints(net["connections"]))
    for name in data["components"]:
        write_component(svg, name, data["components"][name], 'b')
    if svg.mylabels:
        svg.add(svg.mylabels)
    if flip_y:
        els = svg.elements[:]
        svg.elements.clear()
        inv = svg.g(transform=f'translate(0, {COORD_SIZE[1]}) scale(1, -1)')
        for el in els:
            inv.add(el)
        svg.add(inv)
    if dest:
        svg.save(pretty=True)
    else:
        return svg.tostring()

def save_svg_as_pdf(dest, svg):
    svg_file = tempfile.NamedTemporaryFile(delete=False)
    svg_file.write(svg)
    svg_file.close()
    svg_file = Path(svg_file.name)
    rlg = svg2rlg(svg_file)
    renderPDF.drawToFile(rlg, str(dst_file.absolute()))
    svg_file.unlink()

def convert_file(src_file, dst_file, draw_tracks, draw_PWR_GND, labels):
    print(f"Writing SVG for {src_file} to {dst_file}")
    assert dst_file.suffix == '.svg' or dst_file.suffix == '.pdf'

    if src_file.name.endswith('pdes.json') or src_file.name.endswith('kicad_pcb'):
        import pcbenv
        env = pcbenv.make("pcb-v2")
        if not env.set_task({'pdes': str(src_file.absolute()), 'load_tracks': draw_tracks, 'no_polygons': False, 'rectify_degrees': 0.5}):
            raise Exception(f"Failed to load board: {src_file}")
        data = env.get_state({'board': 4})['board']
        env.close()
    else:
        with src_file.open("r") as f:
            data = json.load(f)

    if dst_file.suffix == '.svg':
        svg_file = dst_file
    else:
        svg_file = tempfile.NamedTemporaryFile(delete=False)
        svg_file.close()
        svg_file = Path(svg_file.name)

    des2svg(data, None, svg_file, draw_tracks=draw_tracks, draw_PWR_GND=draw_PWR_GND, labels=labels)

    if dst_file.suffix == '.pdf':
        rlg = svg2rlg(svg_file)
        renderPDF.drawToFile(rlg, str(dst_file.absolute()))
        svg_file.unlink()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument("boards", type=str, nargs='+', help="Input files")
    parser.add_argument("-o",     type=str,            required=False, help="Output file", metavar="dir")
    parser.add_argument("--pdf",  action='store_true', required=False, help="Save as PDF")
    parser.add_argument("-t",     action='store_true', required=False, help="Draw tracks")
    parser.add_argument("-p",     action='store_true', required=False, help="Draw power/ground nets")
    parser.add_argument("-l",     action='store_true', required=False, help="Draw labels")

    args = parser.parse_args()

    src_files = []
    for s in args.boards:
        f = Path(s)
        if f.is_dir():
            src_files += [Path(s) for s in f.glob('*.json')]
            src_files += [Path(s) for s in f.glob('*.kicad_pcb')]
        elif f.exists():
            src_files.append(f)
        else:
            raise Exception(f"source file {f} does not exist")

    suf = '.pdf' if args.pdf else '.svg'

    dst_path = Path(args.o) if args.o else None
    if args.o and not dst_path.is_dir():
        if len(src_files) > 1:
            raise Exception("specified 1 output file for multiple sources")
        dst_files = [dst_path]
    elif args.o:
        dst_files = [dst_path / (s.name + suf) for s in src_files]
    else:
        dst_files = [s.parent / (s.name + suf) for s in src_files]

    for s, d in zip(src_files, dst_files):
        if d.suffix != suf:
            raise Exception("please match output file suffix to format")
        convert_file(s, d, draw_tracks=args.t, draw_PWR_GND=args.p, labels=args.l)
