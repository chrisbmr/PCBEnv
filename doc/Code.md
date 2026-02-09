C++ objects
===========

## Composition tree (not inheritance)

    PCBoard  
    ├─ LayoutArea (1)  
    ├─ Layer (N)  
    ├─ Component (N)  
    │  ├─ AShape (1)  
    │  └─ Pin (N)  
    │     └─ AShape (1)  
    ├─ Net  
    │   ├─>Pin (N)  
    │   └─ Connection (N)  
    │      ├─>Pin (2)  
    │      └─ Track (N)  
    │         ├─ WideSegment_25 (N)  
    │         └─ Via (N)  
    ├─ NavGrid (1)  
    │  └─ NavPoint (N)  
    └─ BVH (1)  
       └─ Object (N)  
   
    EnvPCB  
    ├─ PCBoard (1)  
    ├─ Agent (1)  
    └─ UIApplication (1)  
   
    Agent  
    ├─>PCBoard (1)  
    ├─ ActionSpace (1)  
    ├─ RewardFunction (1)  
    ├─ Policy (N)  
    └─>Python Interface Object (1)  
   
    QPCBApplication : UIApplication : IUIApplication  
    ├─ QPCBEventReceiver (1)  
    ├─ Window : QWidget (1)  
    │  ├─>GLWidget : QOpenGLWidget, QOpenGLFunctions_4_3_Core  
    │  ├─ UIActions  
    │  ├─ ActionTab : QWidget  
    │  ├─ BrowserTab : QWidget  
    │  ├─ ViewTab : QWidget  
    │  └─ ParameterTab : QWidget  
    ├─>PCBoard (1)  
    └─>Agent (2)  


### [PCBoard](/pcbenv/cxx/PCBoard.hpp)
The printed circuit board object.

### [Layer](/pcbenv/cxx/Layer.hpp)
Describes a layer in the PCB.
For now, this is only a number and a designated type (signal, ground, power, or mixed).

### [LayoutArea](/pcbenv/cxx/LayoutArea.hpp)
Describes the routable area of the PCB.
For now, this is always a rectangle.

### [AShape](/pcbenv/cxx/AShape.hpp)
A generic geometric 2d shape (circle, triangle, iso rectangle, or wide segment).
Only wide segments have a z-coordinate.

### [Object](/pcbenv/cxx/Object.hpp)
The base class of Component and Pin.
Has an absolutely positioned AShape.

### [Component](/pcbenv/cxx/Component.hpp)
A circuit component.
Contains a set of pins.

### [Pin](/pcbenv/cxx/Pin.hpp)
A pin.
Can be member of a net and participate in multiple [Connections](/doc/Code.md/#connection).

### [Net](/pcbenv/cxx/Net.hpp)
A collection of pins that should be electrically connected.
Contains a set of point-to-point connections.

### [Connection](/pcbenv/cxx/Connection.hpp)
A point-to-point connection to be realized with a continguous track.
Contains one or more tracks.

### [Track](/pcbenv/cxx/Track.hpp)
A wide polyline in 3D space.
Contains a list of consecutive [wide segments](/doc/Code.md#widesegment_25) and a set of [vias](/doc/Code.md#via), one for each layer change.

### [Path](/pcbenv/cxx/Path.hpp)
An auxiliary structure specifying a track as a list of points.
Does not contain vias.

### [Point_25](/pcbenv/cxx/Geometry.hpp)
A point in 2.5D space: a CGAL Point_2 with an integer z-coordinate.

### [WideSegment_25](/pcbenv/cxx/AShape.hpp)
Represents a wide line segment (specifically, the Minkowski sum of a line segment and a circle).
A subclass of AShape.

### [Segment_25](/pcbenv/cxx/Geometry.hpp)
A line segment (without width) in 2.5D space: a CGAL Segment_2 with an integer z-coordinate.

### [Via](/pcbenv/cxx/Via.hpp)
Vias are necessary for tracks to move in the z-direction.
A via is a circular copper-plated hole in the board and is thus defined by a Circle_2 shape and a minimum and maximum z-coordinate.

### [DesignRules](/pcbenv/cxx/DesignRules.hpp)
A helper object summarizing 3 parameters subject to design rules: clearance, trace width, and via diameter/radius.

### [BVH](/pcbenv/cxx/BVH.hpp)
A bounding volume hierarchy, to quickly find components and pins by location or bounding box.
Implemented as an [AABBTree](/pcbenv/cxx/AABBTree.cpp).

### [NavGrid](/pcbenv/cxx/NavGrid.hpp)
The 3D routing grid.
A subclass of [UniformGrid25](/pcbenv/cxx/UniformGrid25.hpp).

### [NavPoint](/pcbenv/cxx/NavPoint.hpp)
A grid cell or "navigation point" on the routing grid.

### [GridDirection](/pcbenv/cxx/GridDirection.hpp)
Helper class representing one of the 8+2 directions on the routing grid (45-degree steps in the xy-plane plus the z-axis).

### [EnvPCB](/pcbenv/cxx/EnvPCB.hpp)
The C++ side of the Python environment interface object.

### [Geometric objects](/pcbenv/cxx/Geometry.hpp)
Helpers for CGAL objects such as Bbox_2, Segment_2, Circle_2, Iso_rectangle_2, Polygon_2.

### [UserSettings](/pcbenv/cxx/UserSettings.hpp)
The configuration options specified in the [settings JSON file](/pcbenv/data/settings.json).
See the [schema](/pcbenv/data/settings.schema.json) for a description.

### [Rasterizer](/pcbenv/cxx/Rasterizer.hpp)
The base class for rasterizing geometric shapes to a grid.

### [DRCViolation](/pcbenv/cxx/DRC.hpp)
Result of a Design Rule Check.

### [Color](/pcbenv/cxx/Color.hpp)
A 32-bit RGBA color for the UI.

### [CloneEnv](/pcbenv/cxx/Clone.hpp)
Helper for deep-copying a PCBoard.

### [Length](/pcbenv/cxx/Length.hpp)
Helper for length units (nanometers, micrometers, etc.).

### [Agent](/pcbenv/cxx/RL/Agent.hpp)
Base class for C++ agents.
This also doubles as an MDP class because it references an action space and reward function.

### [UserAgent](/pcbenv/cxx/RL/UserAgent.hpp)
The "user" agent that implements the default MDP (`env.step()` and `env.get_state()`) when no other agent has been set via `env.set_agent()`.

### [Action](/pcbenv/cxx/RL/Action.hpp)
Base class for MDP actions (in [cxx/RL/Actions/](/pcbenv/cxx/RL/Actions/)] invoked via `env.step()` or by a custom C++ agent for manipulating the PCB and related state.
See the [action documentation](/doc/Actions.md).

### [ActionSpace](/pcbenv/cxx/RL/ActionSpace.hpp)
A collection of actions.

### [RewardFunction](/pcbenv/cxx/RL/RewardFunction.hpp)
Base class for reward functions.

### [StateRepresentation](/pcbenv/cxx/RL/StateRepresentation.hpp)
Base class for state representations that return a Python object.

### [Policy](/pcbenv/cxx/RL/Policy.hpp)
Base class for C++ agent policies.


Other source files/directories
==============================
- [cxx/AStar.hpp](/pcbenv/cxx/AStar.hpp) Implementation of A-star.
- [cxx/Defs.hpp](/pcbenv/cxx/Defs.hpp) Some common definitions such as `using uint = unsigned int`.
- [cxx/Loaders/*](/pcbenv/cxx/Loaders) Code for loading KiCAD and JSON boards.
- [cxx/Loaders/RouteTracker.hpp](/pcbenv/cxx/Loaders/RouteTracker.hpp) Code for stitching loose collections of segments and vias into polylines as Tracks.
