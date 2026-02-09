PCBEnv2: Printed Circuit Board Machine Learning Environment
===========================================================

Usage
-----
See the [API documentation](/doc/API.md), as well as [pcbenv/playground.py](/pcbenv/playground.py) and [pcbenv/tests/](/pcbenv/tests/) for examples.

Example Boards
--------------
Run *pcbenv/data/boards/download.py* to auto-fetch some boards for testing.

UI Controls
-----------
- Hold right mouse button to scroll
- Left click on pins to select a connection
- Left click on a pin of the selected connection to reverse the order
- Hold Ctrl to rectangle-select (for Autoroute and Unroute Selection)
- Hold Alt to add to rectangle selection
- Click Shift to activate manual routing tool


Installation (Linux)
--------------------
0. Check dependencies section.
1. git clone https://github.com/chrisbmr/PCBEnv.git
2. cd PCBEnv
3. pip install .

Non-installation (Linux)
-----------------------
2. cd PCBEnv/pcbenv/cxx
3. mkdir build
4. cd build
5. cmake ../ -DENABLE_UI=[1|0]
6. make -j$(nproc) install
8. run your python scripts from the pcbenv directory with PYTHONPATH=$PYTHONPATH:/path/to/PCBEnv


Dependencies
------------
Most of them (with the exception of CGAL) are hopefully already installed [on a development machine] or are fetched automatically by cmake.
Only Python, Qt (optional), and SWIG are not fetched automatically.

### Necessary:
* Python with C headers
* [CGAL](https://www.cgal.org/) (Computational Geometry Algorithms C++ header library)
* [rapidjson](https://rapidjson.org/) (C++ header library)
* [swig](https://www.swig.org/) (C++/Python interface generator)

Required by CGAL:
* [boost](https://www.boost.org/) (Boost C++ header libraries >= 1.74)
Can be used by CGAL but are disabled by default:
* gmp (The GNU MP Bignum Library)
* mpfr (GNU multiple-precision floating-point computations)

### For the user interface (if enabled):
* [Qt6](https://www.qt.io/development/qt-framework) with C++ headers
* OpenGL headers

*NOTE*: CGAL and Qt are free for academic and open source users, so if you read anything about getting a license: no, click elsewhere.

Install with, for example: `pacman -S cgal swig rapidjson qt6-base qt6-tools`

### Fallback on Linux
If you don't have root access and something has failed to install so far, please consider building manually.

For example, do `./configure --prefix=/your/home/...` and edit CMakeLists.txt to look there by uncommenting the obvious bits at the top.
There is a python script to do this in the Dependencies folder but you really shouldn't need to.


Directory structure
-------------------
- `cxx/`   C++ code (see [code documentation](/doc/Code.md) for an overview).
- `data/`  Example boards, [settings file](/pcbenv/data/settings.json), JSON schemas, UI resources.
- `util/`  Python class for the PCB JSON format and JSON to SVG converter.
