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
 * Imports commonly used by the cache simulator.
 */

#ifndef CACHESIM_HPP_
#define CACHESIM_HPP_

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <deque>
#include <string>
#include <stdint.h>
#include <assert.h>

#define NOTNULL(ptr) assert(NULL!=ptr)

/** memory access permissions */
enum MemoryAccessPermission {
  NOTHING_OK = 4, READ_OK = 5, WRITE_OK = 6
};

enum MemoryAccessType {
  READ_ACCESS = 8, WRITE_ACCESS = 9
};

enum SynchronizationOperationType {
  SYNC_SOURCE = 10, SYNC_SINK = 11
};

using namespace std;

#include "MemoryAccess.hpp"

#endif /* CACHESIM_HPP_ */
