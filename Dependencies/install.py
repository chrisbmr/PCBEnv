import argparse
import pathlib
import shutil
import os
import distutils
import urllib.request
from termcolor import colored

# NOTE: shutil.unpack_archive() does not preserve permissions

ROOT = pathlib.Path().absolute()

INCLUDE_DIR = ROOT / "include"

DEPS = {
    'rapidjson':
    {
        'url': "https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip",
        'arf': ROOT / "rapidjson-v1.1.0.zip",
        'dir': ROOT / "rapidjson-1.1.0",
        'chk': INCLUDE_DIR / "rapidjson"
    },
    'swig':
    {
        'url': "http://prdownloads.sourceforge.net/swig/swig-4.2.1.tar.gz",
        'arf': ROOT / "swig-4.2.1.tar.gz",
        'dir': ROOT / "swig-4.2.1"
    },
    'gmp':
    {
        'url': "https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz",
        'arf': ROOT / "gmp-6.3.0.tar.xz",
        'dir': ROOT / "gmp-6.3.0"
    },
    'mpfr':
    {
        'url': "https://www.mpfr.org/mpfr-current/mpfr-4.2.1.zip",
        'arf': ROOT / "mpfr-4.2.1.zip",
        'dir': ROOT / "mpfr-4.2.1"
    },
    'boost':
    {
        'url': "https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.tar.bz2",
        'arf': ROOT / "boost_1_85_0.tar.bz2",
        'dir': ROOT / "boost_1_85_0"
    },
    'cgal':
    {
        'url': "https://github.com/CGAL/cgal/releases/download/v5.6.1/CGAL-5.6.1.zip",
        'arf': ROOT / "CGAL-5.6.1.zip",
        'dir': ROOT / "CGAL-5.6.1"
    },
    'eigen3':
    {
        'url': "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip",
        'arf': ROOT / "eigen-3.4.0.zip",
        'dir': ROOT / "eigen-3.4.0"
    }
}

parser = argparse.ArgumentParser()

parser.add_argument("names", nargs='+', choices=list(DEPS.keys()), help="Libraries to download and install into "+str(ROOT))

_args = parser.parse_args()

print("---\nWill install the following into " + str(ROOT))
for name in DEPS:
    print(colored(name, 'green' if name in _args.names else 'red') + ":", name in _args.names)
input("---\nEnter to continue.")

def fetch_files():
    print("Fetching sources...")
    for u in _args.names:
        if DEPS[u]['arf'].is_file():
            print(DEPS[u]['arf'].name, "exists")
        else:
            print("Downloading", DEPS[u]['url'])
            urllib.request.urlretrieve(DEPS[u]['url'], DEPS[u]['arf'])

def install_rapidjson(RJ):
    if RJ['chk'].is_dir():
        print("rapidjson exists")
        return
    print("Installing rapidjson headers ...")
    shutil.unpack_archive(RJ['arf'])
    shutil.copytree(RJ['dir'] / "include", INCLUDE_DIR, dirs_exist_ok=True)
    shutil.rmtree(RJ['dir'])

def build_swig(SWIG):
    if (ROOT / "share" / "swig").is_dir():
        print("swig exists")
        return
    print("Building swig...")
    distutils.spawn.spawn(['tar', '-xzf', str(SWIG['arf'])])
    os.chdir(SWIG['dir'])
    distutils.spawn.spawn(['sh', 'configure', '--prefix='+str(ROOT)])
    distutils.spawn.spawn(['make', '-j6'])
    distutils.spawn.spawn(['make', 'install'])
    os.chdir(ROOT)
    shutil.rmtree(SWIG['dir'])

def build_gmp(GMP):
    if (ROOT / "lib" / "libgmp.so").is_file():
        print("GMP exists")
        return
    print("Building GMP ...")
    distutils.spawn.spawn(['tar', '-xf', str(GMP['arf'])])
    os.chdir(GMP['dir'])
    distutils.spawn.spawn(['sh', 'configure', '--prefix='+str(ROOT)])
    distutils.spawn.spawn(['make', '-j6'])
    distutils.spawn.spawn(['make', 'install'])
    os.chdir(ROOT)
    shutil.rmtree(GMP['dir'])

def build_mpfr(MPFR):
    if (ROOT / "lib" / "libmpfr.so").is_file():
        print("MPFR exists")
        return
    print("Building MPFR")
    distutils.spawn.spawn(['unzip', str(MPFR['arf'])])
    os.chdir(MPFR['dir'])
    distutils.spawn.spawn(['sh', 'configure', '--prefix='+str(ROOT), '--with-gmp='+str(ROOT)])
    distutils.spawn.spawn(['make', '-j6'])
    distutils.spawn.spawn(['make', 'install'])
    os.chdir(ROOT)
    shutil.rmtree(MPFR['dir'])

def install_boost(BOOST):
    if (INCLUDE_DIR / "boost").is_dir():
        print("Boost exists")
        return
    print("Installing boost headers ...")
    shutil.unpack_archive(BOOST['arf'])
    if False:
        shutil.copytree(BOOST['dir'] / "boost", INCLUDE_DIR / "boost", dirs_exist_ok=True)
    else:
        os.chdir(BOOST['dir'])
        distutils.spawn.spawn(['sh', 'bootstrap.sh'])
        distutils.spawn.spawn(['b2', "install", "--with-graph", "--prefix="+str(ROOT)])
        os.chdir(ROOT)
    shutil.rmtree(BOOST['dir'])

def install_CGAL(CGAL):
    if CGAL['dir'].is_dir():
        print("CGAL exists")
        return
    print("Installing CGAL headers ...")
    shutil.unpack_archive(CGAL['arf'])

def install_eigen3(EIGEN3):
    if (INCLUDE_DIR / "Eigen").is_dir():
        print("Eigen3 exists")
        return
    print("Installing Eigen3 headers ...")
    shutil.unpack_archive(EIGEN3['arf'])
    shutil.copytree(EIGEN3['dir'] / "Eigen", INCLUDE_DIR / "Eigen", dirs_exist_ok=True)
    shutil.rmtree(EIGEN3['dir'])

fetch_files()
if 'swig' in _args.names:
    build_swig(DEPS['swig'])
if 'gmp' in _args.names:
    build_gmp(DEPS['gmp'])
if 'mpfr' in _args.names:
    build_mpfr(DEPS['mpfr'])
if 'rapidjson' in _args.names:
    install_rapidjson(DEPS['rapidjson'])
if 'boost' in _args.names:
    install_boost(DEPS['boost'])
if 'cgal' in _args.names:
    install_cgal(DEPS['cgal'])
if 'eigen3' in _args.names:
    install_eigen3(DEPS['eigen3'])
