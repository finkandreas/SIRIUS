#ifndef __SIRIUS_H__
#define __SIRIUS_H__

#define stop(info_string) { info_string; std::cout << std::endl << "  line " << __LINE__ << " of file " \
  << __FILE__ << std::endl; throw std::runtime_error("Abort execution");}

#include <assert.h>
#include <stdio.h>

#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>

extern "C" {
#include <spglib.h>
}

#include "error_handling.h"
#include "config.h"
#include "constants.h"
#include "mdarray.h"
#include "linalg.h"
#include "radial_grid.h"
#include "spline.h"
#include "radial_solver.h"
#include "json_tree.h"
#include "atom_type.h"
#include "atom_symmetry_class.h"
#include "atom.h"
#include "global.h"

#endif // __SIRIUS_H__
