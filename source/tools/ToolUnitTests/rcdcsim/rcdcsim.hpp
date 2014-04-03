/*
  RCDC-sim: A Relaxed Consistency Deterministic Computer simulator
  Copyright 2011 University of Washington

  Contributed by Joseph Devietti

This file is part of RCDC-sim.

RCDC-sim is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RCDC-sim is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCDC-sim.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * common includes for the back-end simulator
 */

#ifndef RCDCSIM_HPP_
#define RCDCSIM_HPP_

#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>

#include <vector>
#include <map>
#include <set>
#include <deque>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <stdint.h>

using namespace std;

// TODO: hack to emulate this value from pin.H; remove once we no longer track sync sink/sources?
#define INVALID_THREADID (static_cast<uint32_t>(-1))

#endif /* RCDCSIM_HPP_ */
