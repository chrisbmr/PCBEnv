PACKAGE MANAGER
===============

Arch Linux / Pacman
----------
pacman -S core/mpfr
pacman -S core/gmp
pacman -S extra/rapidjson
pacman -S extra/swig
pacman -S extra/boost
pacman -S extra/qt6-base
pacman -S extra/qt6-tools

Ubuntu
------
libmpfr-dev
libgmp-dev
swig
rapidjson-dev
libboost-dev
qt3d5-dev (?)

LINKS
=====

swig
----
https://www.swig.org/
http://prdownloads.sourceforge.net/swig/swig-4.2.1.tar.gz
http://prdownloads.sourceforge.net/swig/swigwin-4.2.1.zip

CGAL (header-only library, fetched by cmake)
----
https://www.cgal.org/
https://github.com/CGAL/cgal/releases/tag/v5.6.1
https://github.com/CGAL/cgal/releases/download/v5.6.1/CGAL-5.6.1.zip
https://github.com/CGAL/cgal/releases/download/v5.6.1/CGAL-5.6.1-win64-auxiliary-libraries-gmp-mpfr.zip

boost (header-only library, fetched by cmake)
-----
https://www.boost.org/
https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.zip
https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.tar.bz2

gmp (disabled by default)
---
https://gmplib.org/
https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz

mpfr (disabled by default)
----
https://www.mpfr.org/
https://www.mpfr.org/mpfr-current/mpfr-4.2.1.zip

rapidjson (fetched by cmake)
---------
https://rapidjson.org/
https://github.com/Tencent/rapidjson/
https://github.com/Tencent/rapidjson/releases/tag/v1.1.0
https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip

Qt (for UI only)
--
https://www.qt.io/
https://www.qt.io/download-open-source

eigen3 (fetched by cmake)
------
https://eigen.tuxfamily.org/
https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip