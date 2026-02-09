# Action Specifications

The following are accepted by `env.step(...)`:


---
## `astar`
Route the specified connection with A-star.  
Removes existing tracks.

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
2. A dictionary to customize the [A-star cost function](/doc/API.md#a-star-costs). Defaults are substituted for missing members. WARNING: Costs are remembered and used in subsequent calls without specified costs. Use `{}` to reset.

**Examples**

- `step(('astar', ('IC1-A', 'IC2-A')))`
- `step(('astar', (('IC1-A', 'IC2-A'), {'wd': 4, 'dir': 'xy yx'})))`

---
## `astar_to`
Route from the current track endpoint to an arbitrary point with A-star.

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
2. The starting point as a `tuple(x,y,z)`, or None to use the connection's source point or the endpoint of the connection's track. Throws if there is more than 1 track.
3. The target point as a `tuple(x,y,z)`.
4. OPTIONAL: A dictionary to customize the A-star cost function. See 'astar' action.

**Examples**

- `step(('astar_to', (('IC1-A', 'IC2-A'),None,(5.0,-5.0,1))))`
- `step(('astar_to', (('IC1-A', 'IC2-A'),None,(5.0,-5.0,1), {'45': 0.125})))`

---
## `segment_to`
Add a segment or via to the connection if it does not violate any clearance rules.

**Returns** `success=False` if the segment violates clearance rules and does not add the segment. Returns the reward for the added segment (or 0).

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
2. The starting point as a `tuple(x,y,z)`, or None to use the connection's source point or the endpoint of the connection's track. Throws if there is more than 1 track.
3. The target point as a `tuple(x,y,z)`.

**Examples**

- `step(('segment_to', (('IC1-A', 'IC2-A'),(5.0,-5.0,1))))`

---
## `unroute`
Unroute a connection.

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`

**Examples**

- `env.step(('unroute', ('IC1-A', 'IC2-A')))`

---
## `unroute_net`
Unroute an entire net.

**Arguments**

1. The net's name

**Examples**

- `env.step(('unroute_net', 'GND'))`

---
## `unroute_segment`
Unroute the last segment or via (determined automatically) of a track.

**Returns** `success=False` if the track was empty or no track was found ending near the specified point.

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
2. The endpoint (+- 1 units in each coordinate) of the track as a `tuple(x,y,z)` or `None` (if there is no ambiguity). Cannot be omitted.

**Examples**

- `step(('unroute_segment', (('IC1-A', 'IC2-A'), None)))`
- `step(('unroute_segment', (('IC1-A', 'IC2-A'), (0.5,0.5,1))))`

---
## `set_track`
Set the track for a connection.  
This will become its sole track, existing ones are erased.

**Arguments**

1. The connection as `('com-pin','com-pin')` or `('net', index: int)`
2. The track as a dictionary `{ 'segments': [(x0,y0,x1,y1,z,w), ...], 'vias': [((x,y),zmin,zmax,r), ...] }`. The lists may also be 2D float32 NumPy arrays of shape `(n,6)` and `(n,5)`, respectively, or 1D flattened versions. For Python lists of tuples, the z-values must be integers.

**Examples**

- `env.step(('set_track', (('IC1-A','IC2-A'), {'segments': [(0.5,0.5, 1.5,0.5, 0, 0.5),(1.5,0.5, 1.5,1.5, 2, 1.5)], 'vias': [((1.5,0.5), 0,2, 1.0)]})))`
- `env.step(('set_track', (('IC1-A','IC2-A'), {'vias': [((5.0,5.0), 0,3, 0.25)]})))`
- `env.step(('set_track', (('GND', 0), {'segments': np.array(segments, dtype=np.float32)})))`

---
## `set_guard`
Set a routing guard path for A-star, i.e. a 1D curve that blocks grid cells (even when the violation cost is finite).  
Always clears the old guard path.

**Arguments**

1. A path as a list of n 2.5D points as 3-tuples `(number,number,int)` or a 2D f32 NumPy array of size `(n,3)` or a 1D f32 NumPy array of size `3n`, or `None`. Changing layer starts a new path. Paths with only a single point are ignored.

**Examples**

- `env.step(('set_guard', None))`
- `env.step(('set_guard', [(0,0,0),(1,0,0),(0,0,1),(1,0,1)])`
- `env.step(('set_guard', np.array([0,0,0, 1,0,0, 0,0,1, 1,0,1], dtype=np.float32))`
- `env.step(('set_guard', np.array([[0,0,0],[1,0,0],[0,0,1],[1,0,1]], dtype=np.float32))`
- `env.step(('set_guard', [(0,0,0),(1,0,0),(0,0,-1),(0,1,0),(1,1,0)])`

---
## `set_layer_mask`
Set the layer mask for a specific net.  
The layer mask is used in the grid cell cost calculation of A-star.

**Arguments**

1. A bit mask as an integer or a list of booleans. Lengths > 64 are silently ignored. Cannot be a NumPy array.

**Examples**

- `env.step(('set_layer_mask', 0x6))`
- `env.step(('set_layer_mask', [False,True,True,False]))`

---
## `set_costs`
Set the grid cell base costs for a section of the grid.

**Arguments**

1. None to set the cost to 1 everywhere


or

1. A 3D integer grid location as a `tuple(x,y,z)`
2. A 3D NumPy float32 array


or

1. A 3D NumPy float32 array with the dimensions of the grid, optionally flattened to 1D.
1. The scalar cost value to be assign to a region.
2. The minimum point of a 3D integer bounding box defining the region as a `tuple(x,y,z)`
3. The maximum point

**Examples**

- `env.step(('set_costs', None))`
- `env.step(('set_costs', (0,0,1), np.array([[[1,1,1],[1,1,1]]], dtype=np.float32)))`
- `env.step(('set_costs', 1.1, (0,0,0), (10,10,1)))`
- `env.step(('set_costs', np.ones(grid_shape)))`

---
## `lock_routed`
Set the lock status of all connections with tracks (even if incomplete) to `<argument>`, and of all others to `False`.  
This affects reset() but otherwise is only for use with custom agents.

**Arguments**

1. `boolean`

**Examples**

- `env.step(('lock_routed', True))`
