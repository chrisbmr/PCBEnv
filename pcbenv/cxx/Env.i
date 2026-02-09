%module EnvPCB

%newobject create_env;

%{
#include "Env.hpp"
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define PY_ARRAY_UNIQUE_SYMBOL _pcbenv2_module_PyArray_API
#include <numpy/ndarrayobject.h>
%}
%include "Env.hpp"

%init %{
import_array();
%}
