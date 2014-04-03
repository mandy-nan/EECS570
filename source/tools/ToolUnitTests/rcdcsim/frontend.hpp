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

#ifndef FRONTEND_H_
#define FRONTEND_H_

#include "pin.H"

#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>

#include <vector>
#include <map>
#include <set>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <stdint.h>

#define VERBOSE(...)
#define VERBOSE_SYNC(...)
#define VERBOSE_MEM(...)
#define VERBOSE_ALLOC(...)

//#define VERBOSE fprintf
//#define VERBOSE_SYNC fprintf
//#define VERBOSE_MEM fprintf
//#define VERBOSE_ALLOC fprintf

/** Wrapper for Pin locks to make them a bit simpler to use. Automatically
 * initializes the lock and avoids false sharing. */
class PinLock {
private:
  PIN_LOCK m_pinlock;
  char padding[64 - sizeof(PIN_LOCK)]; // pad to 64B cache line

public:
  PinLock() {
    InitLock( &m_pinlock );
    //assert( 0 == m_pinlock );
  }

  /** Acquire this lock. */
  void lock() {
    VERBOSE(stderr, "acquiring lock %p\n", &m_pinlock);
    GetLock(&m_pinlock, 4);
    //assert( 0 != m_pinlock );
    /*
    while ( true ) {
      if ( 0 == m_pinlock ) {
        GetLock(&m_pinlock, 4); // try to acquire
        return;
      } else {
        PIN_Yield();
      }
    }
    */
  }

  /** Release this lock. */
  void unlock() {
    VERBOSE(stderr, "releasing lock %p\n", &m_pinlock);
    //assert( 0 != m_pinlock );
    ReleaseLock(&m_pinlock);
  }
};

using namespace std;

enum DepGranularity {
  WordDep = 2, DoubleWordDep = 3, QuadWordDep = 4, Line32Dep = 5, PageDep = 12
};

#endif /* FRONTEND_H_ */
