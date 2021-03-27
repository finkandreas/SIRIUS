#ifndef PYTHON_MODULE_INCLUDES_H
#define PYTHON_MODULE_INCLUDES_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <sirius.hpp>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <pybind11/complex.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <stdexcept>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "utils/json.hpp"
#include "unit_cell/free_atom.hpp"
#include "dft/energy.hpp"
#include "magnetization.hpp"
#include "unit_cell_accessors.hpp"
#include "make_sirius_comm.hpp"
#include "beta_projectors/beta_projectors_base.hpp"
#include "hamiltonian/inverse_overlap.hpp"
#include "preconditioner/ultrasoft_precond.hpp"

using namespace pybind11::literals;
namespace py = pybind11;
using namespace sirius;
using namespace geometry3d;
using json = nlohmann::json;

using nlohmann::basic_json;

#endif /* PYTHON_MODULE_INCLUDES_H */
