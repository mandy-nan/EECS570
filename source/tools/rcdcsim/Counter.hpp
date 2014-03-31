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

#ifndef COUNTER_HPP_
#define COUNTER_HPP_

#include <iostream>
#include <vector>
#include <string>
#include <stdint.h>
#include <assert.h>

class Counter {
private:
  unsigned m_cpuid;
  uint64_t m_stat;
  std::string m_name;

  /** List of all the stats that have been created. */
  static std::vector<Counter*> s_AllStats;

public:

  static void dumpCounters(std::ostream& os, const std::string& prefix, const std::string& suffix) {
    std::vector<Counter*>::iterator it = s_AllStats.begin();
    for ( ; it != s_AllStats.end(); it++ ) {
      os << prefix;
      os << "'cpuid': " << (*it)->m_cpuid << ", ";
      os << "'" << (*it)->m_name << "': " << (*it)->m_stat;
      os << suffix;
    }
  }

  Counter(unsigned cpuid, const char* name) {
    s_AllStats.push_back( this );
    m_cpuid = cpuid;
    m_stat = 0;
    m_name = name;
  }

  uint64_t get() {
    return m_stat;
  }
  void set(uint64_t v) {
    m_stat = v;
  }

  uint64_t operator=( uint64_t v ) {
    m_stat = v;
    return m_stat;
  }
  /*
  // abortive attempt at supporting addition of Counters
  uint64_t operator+(const Counter& c) {
    return m_stat + c.m_stat;
  }
  uint64_t operator+(uint64_t v) {
    return m_stat + v;
  }
  */

  void operator++( const int ignore ) {
    m_stat++;
  }
  uint64_t operator+=( const int v ) {
    m_stat += v;
    return m_stat;
  }
  bool operator==( const Counter& c ) const {
    return ( m_stat == c.m_stat );
  }
  bool operator==( const int v ) const {
    return ( m_stat == (uint64_t) v );
  }
  bool operator!=( const int v ) const {
    return ( m_stat != (uint64_t) v );
  }
  friend std::ostream& operator<<( std::ostream& os, const Counter& c ) {
    return os << c.m_stat;
  }

};

#endif /* COUNTER_HPP_ */
