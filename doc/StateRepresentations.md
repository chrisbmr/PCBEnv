# State Representations

The following are accepted by `env.get_state(...)`:


---
## `grid`
The state of the routing grid in the form of a 3D uint16 NumPy array of the grid cell flags.

**Parameters**

A 3D integer bounding box of grid coordinates `((xmin,ymin,zmin),(xmax,ymax,zmax))`, or `None` for the whole grid.

**Examples**

- `env.get_state({'grid': None})`
- `env.get_state({'grid': ((0,0,0),(8,8,0))})`

---
## `ends`
A 2D NumPy float32 array of shape `(N,6)` specifying the 2 endpoints `(x0,y0,z0,x1,y1,z1)` of all `N` connections on the board.

**Parameters**

None

**Examples**

- `env.get_state({'env': None})`

---
## `features`
Return a set of features for each connection of the board.  
See [feature description](/doc/API.md#hand-crafted-state-features).

**Parameters**

`None` or `{ 'as_dict': boolean }` to specify whether to return a dictionary or a NumPy array. Defaults to NumPy.

**Examples**

- `env.get_state({'features': None})`
- `env.get_state({'features': {'as_dict': True}})`

---
## `metric`
Compute a board metric.

**Parameters**

1. 'ratsnest_crossings'
1. 'ratsnest_crossings'
2. an iterable of net names

**Examples**

- `env.get_state({'metric': 'ratsnets_crossings'})`
- `env.get_state({'metric': ('ratsnets_crossings', {'Net1', 'Net2'})})`

---
## `clearance_check`
Check specified items for clearance violations (track-track or track-pin) with all items on the board (except for pins without nets).

A list of clearance violations: `[{'x': (x,y,z), 'connections': (connection-ref 1, connection-ref 2 or None), 'pin': pin-ref }, ...]`. The indicated point will be near the violation but may not be exact.

**Parameters**

A dictionary specifying objects, a limit, and whether to check in 2D or 3D: `{'nets': [...], 'components': [...], 'pins': [...], 'connections': [...], '2d': boolean, 'limit': int}`. All members may be omitted. Nets and components are converted to connections, pins must be listed explicitly.

**Examples**

- `env.get_state({'clearance_check': {'nets': ['GND']}})`

---
## `track`
Get the tracks for a list of connections.

A list (for connections) of lists (for their tracks): `[[{'segments': NumPy array, 'vias': Numpy array}]]`

**Parameters**

List of connection-refs.

**Examples**

- `env.get_state({'track': [('IC1-A','IC2-A'), ('IC1-B','IC2-B')]})`

---
## `raster`
Get the grid cells occupied by the center line of a connection's tracks, for a list of connections.

A list (for connections) of lists (for their tracks) of Numpy arrays of point coordinates: `[[np.array([[x0,y0,z0], [x1,y1,z1], ...])]]`

**Parameters**

List of connection-refs

**Examples**

- `env.get_state({'raster': [('IC1-A','IC2-A'), ('IC1-B','IC2-B')]})`

---
## `board`
Get a dictionary representation of the board in the same format as the [JSON specification](/pcbenv/data/pcb.schema.json) (except some items may be tuples or NumPy arrays instead of lists).

**Parameters**

Integer recursion depth. For instance, if this is 0, only net names rather than their properties are returned.

**Examples**

- `env.get_state({'board': 1})`

---
## `select`
Select a list of items that intersect with either a bounding box or point.

A list of object names.

**Parameters**

`{ 'object': 'pin' | 'component', 'box': ((xmin,ymin,zmin),(xmax,ymax,zmax)), 'pos': (x,y,z) }`.

If `'box'` is specified, `'pos'` is ignored. Throws an exception if `'object'` is missing or both `'pos'` and `'box'` are missing.

**Examples**

- `env.get_state({'select': {'object': 'pin', 'pos': (10,10,0) }})`
- `env.get_state({'select': {'object': 'com', 'pos': (10,10,0) }})`
- `env.get_state({'select': {'object': 'pin', 'box': ((10,10,0),(10,10,1)) }})`

---
## `image_draw|image_grid`
Return an [image representation](/doc/API.md#image-representation) of the board as described in the respective section.  
The image is either rasterized directly to the image or obtained by downscaling the current grid.  
The expansions for A-star are set to 0 for this, but note that each object's own clearance area is still included.  
All arguments are optional.  

*Note*: Parameters are remembered for subsequent calls (for `draw` and `grid` separately).

**Parameters**

A dictionary `{`  

`'bbox'`: The source region of for the image as a 2D box `((xmin,ymin),(xmax,ymax))`.

`'size'`: The size of the image as `(int,int)`. This is used only if `max_size` is set to `(0,0)`. If specified, must be `>= (1,1)`. The returned image array will never be larger than the grid size.

`'max_size'`: The maximum size of the image as `(int,int)`. The source area is downscaled to this size or less (see `max_scale`) if the grid area would not fit.

`'max_scale'`: The maximum scale of the source area. Must be > 0 and <= 1. Does not apply if `max_size` is 0.

`'min_scale'`: The minimum scale of the source area. If `max_size` requires a scale lower than this, an exception is triggered. Must be >= 0 and `<= max_scale`.

`'crop_auto'`: If this is a number `m >= 0`, the bbox is set to the bounding box around all tracks expanded by `m` times its maximum dimension. Set to `True` for `m = 0`, or `False` to disable auto-cropping.

`}`


**Examples**

- `env.get_state({'image_grid': {'crop_auto': 1/16, 'min_scale': 0.125, 'max_scale': 0.5, 'max_size': (384,384)}})`
- `env.get_state({'image_draw': {'bbox': ((0,0),(1024,1024)), 'size': (1024,1024)})`
