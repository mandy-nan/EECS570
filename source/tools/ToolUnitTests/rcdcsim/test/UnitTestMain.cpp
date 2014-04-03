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

/** This file provides the main() function that the Boost Unit Testing Framework uses to
    run all the actual tests. Tests are organized into suites, based on the
    general functionality being examined. There's one suite per file. */

#define BOOST_TEST_MODULE HadronSim Unit Tests
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "Counter.hpp"
std::vector<Counter*> Counter::s_AllStats;

#include "MultiCacheSimulator.hpp"
map<uint64_t, vector<uint64_t> > g_vcOfSyncObject;
