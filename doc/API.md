Environment Methods
===================

### Overview
- [set_task](/doc/API.md#set_tasktask)
- [set_agent](/doc/API.md#set_agentconf)
- [run_agent](/doc/API.md#run_agentargs)
- [reset](/doc/API.md#reset)
- [step](/doc/API.md#stepaction_name-action_args)
- [get_state](/doc/API.md#get_statespec)
- [render](/doc/API.md#rendercommand-str)
- [close](/doc/API.md#close)
- [seed](/doc/API.md#seedvalue)

### `set_task(task)`
Loads a new circuit board.
See the [task JSON schema](/pcbenv/data/task.schema.json) for a usage guide.
The board JSON format is described in a [PCB JSON schema](/pcbenv/data/pcb.schema.json).

### `set_agent(conf)`
Sets the current C++ routing agent and its parameters.
This affects the state and action space and the effect of run_agent().
The described actions and state representations apply to the `"user"` agent only.
The only other agent implemented is `"rrr"` (rip-up and reroute), which only supports `run_agent` but not `reset`/`step`/`get_state`.

### `run_agent(args)`
Let the current routing agent autoroute the board.
Arguments are agent-specific.
The default `"user"` agent does nothing.

### `reset()`
Let the current routing agent reset the state of the board.
The `"user"` agent unroutes all non-locked connections.

### `step((action_name, action_args))`
Perform the specified action: see [action documentation](/doc/Actions.md)

### `get_state(dict)`
Get the specified state representations: see [state representation documentation](/doc/StateRepresentations.md).
For each key in the dict corresponding to a state representation name the output will contain the state representation data under the same key:
`env.get_state({name: parameters})` returns `{name: data}`.

### `render(command: str)`:
`"human"` or `"qt"` start the UI if it has not been started.
`"show"` and `"hide"` operate on the Qt widget.
`"wait"` waits for the user to trigger a lock-stepping event.
Other values are ignored.

### `close()`
Shut down the user interface.

### `seed(value)`
Does nothing for now.


A-star costs
============
A-star uses a cost function that can be configured by passing the following dictionary to the respective actions:

### Parameters dictionary
    {
      'via': number = <settings value>, # via cost
      'drv': number = inf, # design rule violation cost multiplier
      'wd': number = 1, # wrong (non-preferred) direction cost multiplier, must be >= 1
      'wl': number = 4, # wrong (masked) layer cost multiplier
      '45': number = 1/1024, # turn cost per 45 degree angle
      'dir': string = "" # preferred directions, one character per layer, missing layers = ' '
    }

Preferred direction characters: `'x', 'y', or ' ' (no preference)`

### Cost computation

- **0/90 degrees,     preferred:** `1 * cell cost`
- **0/90 degrees, non-preferred:** `1 * cell cost * wrong direction cost`
-   **45 degrees,     preferred:** `sqrt(2) * cell cost`
-   **45 degrees, non-preferred:** `sqrt(2) * cell cost + 1 * cell cost * (wrong direction cost - 1)`
-   **vias:** `via cost * cell cost`

The wrong direction cost multiplier for diagonal segments applies only to the length in one dimension (for simplicity, even when the preferred direction is none), hence the -1.

Then, this value is further multiplied by the wrong layer cost for planar moves on layers where the layer mask bit is 0,
and by the design rule violation cost if the grid cell is already used by another track or within the clearance area.

Finally, a turn cost * ((angle % 360) / 45) is added to prefer straight tracks over zig-zag ones of the same length.



Hand-crafted state features
===========================
These are returned by `get_state({'features': None or {'as_dict': True or False}})`.

There are per-connection features and board-level per-layer features.
The board-level features occupy the first 3 rows of the NumPy array, or are provided under the `'Board'` key of the dictionary.
In the former case, each row represents a feature, and each column a layer (for a maximum of 21 layers).
Non-existent layers are set to `nan`.
In the latter case, there is one NumPy array with <number of layers> elements per feature key.

### Per-connection features in the order of the array's rows:
0. **Routed:** whether the connection is routed (1 or 0)
1. **Track Length:** absolute length in board units as per set_task({resolution_nm:** ...})
2. **Track Length Ratio:** absolute length divided by connection Queen's distance
3. **Number of Vias**
4. **Fraction X:** fraction of the track's length in x-direction
5. **Fraction 45:** fraction of the track's length going diagonally
6. **Fraction Y:** fraction of the track's length in y-direction
7. **Excess Fraction X:** track movement on the x-axis / connection distance on the x-axis - 1
8. **Excess Fraction Y:** see above
9. **RMSD:** root mean square deviation (in board units) of the track from a straight line through the endpoints
10. **MaxDeviation:** maximum distance of the track from the line segment defined by the connection's endpoints
11. **FractionOutsideAABB:** fraction of the track's length outside the axis-aligned bounding box defined by the connection's endpoints
12. **Rat's Nest Intersections:** number of intersections (in 2D) of the connections rat's nest line (if unrouted) or track with other rat's nest lines of unrouted connections
13. **Track Intersections:** number of intersections (in 2D) of the connections rat's nest line (if unrouted) or track with other tracks of routed connections
14. **Winding Angle:** the sum of signed angles between track segments, in radians (π for a U shape, π/2 for an L shape, 0 for a Z shape)
15. **Turning Angle:** the sum of absolute angles between track segments (3π/2 for a Z shape)
16. **Free Space Around Pin 1:** the fraction of grid cells inside the pin's bounding box, expanded by the maximum dimension of the box in both directions, that is not occupied by tracks nor the pin itself; maximum over all layers covered by the pin
17. **Free Space Around Pin 2:** see above
18. **Layer Utilization:** the fraction of layers the connection has track segments on (unaffected by vias)
19. **Minimum Track Length:** the minimum possible track length for the connection (this is not initialized by default and thus 0)
20. **Distance Fraction X:** for the vector (dx,dy) from the connections source to its target point, |dx|/(|dx|+|dy|).

Reported only by dictionary:
1. **`'Net'`**: net name
2. **`'Locked'`**: whether the tracks have been locked with the lock_routed action
3. **`'Source'`**: connection source coordinates
4. **`'Target'`**: connection target coordinates
5. **`'Distance45'`**: the connection Queen's distance (45-degree distance between source and target)

### Per board features:
0. **Layer Bias (XY):** fraction of routing in x-direction minus fraction in y-direction (relative to total routing length on the layer), 0 if layer has no tracks
1. **Layer Fraction 45:** fraction of diagonal routing on each layer (relative to total routing length on the layer), 0 if layer has no tracks
2. **Layer Track Length:** absolute total track length on each layer


Image representation
====================
The image represents a scaled (down but never up) version of the routing grid.
When there is no scaling, each pixel is either filled or empty.
When downscaling, each pixel receives a fractional coverage from source pixels if 'image_grid' is used.
When 'image_draw' is used, regular grid midpoint rasterization rules apply.

Pins and tracks include clearance areas.

Inner layers are compressed into a single tracks channel.
The inner layer tracks channel represents the fraction of inner layers occupied by tracks, multiplied by the fractional coverage from scaling.

Fractions are computed as float32 and then converted to uint8, with 240 (rather than 255) representing a fully covered pixel.
(As 240 is divisble by 1 to 6, 8, 10, 12, 15, 16, ... it provides more exact values for common numbers of inner layers.)

The rat's nest lines are always rasterized as lines (with no width) to the destination image directly, rather than being scaled, and always have full coverage.

### Channel assignment

0. Top Layer Tracks
1. Top Layer Pins
2. Top Layer Rat's Nest
3. Bottom Layer Tracks
4. Bottom Layer Pins
5. Bottom Layer Rat's Nest
6. Inner Layer Tracks Coverage
7. Vias (unused)


# Agent Parameters

The following parameters are understood by the default agent:

    {
      'timeout_us': int = 0, # autoroute timeout in microseconds
      'action_limit': int = 0, # maximum autoroute actions
      'keep_tracks': boolean = True, # remove non-locked tracks on `route_auto()`
      'astar_costs': dict # costs used for A-star
      'reward': dict, # reward function specification
      'py_interface': object # a Python object with policy and value function methods
    }

Parameters for the rip-up and reroute agent:

    {
      'min_iterations': int = 1, # minimum number of passes over all connections
      'max_iterations': int = 256, # maximum number of passes over all connections
      'max_iterations_stagnant': int = 8, # pass without improvement before stopping
      'tidy_iterations': int = 2, # number of tidying passes
      'history_cost_decay': number = 1, # 0 <= decay rate <= 1 (no decay)
      'history_cost_increment': number = 1/16, # increment of the histoy cost
      'history_cost_max': int = 0xfffe, # maximum number of history cost increments
      'randomize_order': boolean = False # whether to randomize the routing order
    }


# Reward Parameters

The following parameters are understood by the default reward (class RL/Reward.*/RouteLength).

    {
      'function': 'track_length',
      'per_unrouted': number, # reward per unrouted connection
      'per_routed': number, # reward per routed connection
      'per_via': number, # reward per via
      'ignore_necessary_vias': number, # whether to count the via necessary when pins are on different layers
      'scale_length': str, # divide track length by one of ['d' for connection Euclidean distance, 'd45' for connection Queen's distance, '' for 1]
      'scale_global': str # divide sum of track lengths by one of ['area_sqrt' for square root of routing area, 'area_diagonal' for the diagonal of the routing area, 'ref_tracks' for the sum of the shortest possible track lengths, 'd_connection' for the sum of Euclidean connection distances, 'd45_connection' for the sum of connection Queen's distances]
    }

