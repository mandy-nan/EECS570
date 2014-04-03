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

#ifndef MEMORYACCESS_HPP_
#define MEMORYACCESS_HPP_


class GenericAccess {
protected:
  MemoryAccessType m_type;
  uint64_t m_addr;
  uint64_t m_baseAddr;
  unsigned m_size;
  uint64_t LINE_SIZE;

public:
  GenericAccess( const MemoryAccessType mtype, const uint64_t addr, const unsigned size,
                 const unsigned LSIZE ) :
    m_type( mtype ), m_addr( addr ), m_baseAddr( addr ), m_size( size ),
        LINE_SIZE( LSIZE ) {
  }
  virtual void operator++( const int unused ) = 0;
  virtual bool keepGoing() const = 0;

  bool write() const {
    return WRITE_ACCESS == m_type;
  }
  bool read() const {
    return READ_ACCESS == m_type;
  }
  MemoryAccessType type() const {
    return m_type;
  }
  unsigned size() const {
    return m_size;
  }

  virtual uint64_t addr() const {
    return m_addr;
  }
  virtual uint64_t lineAddr() const {
    assert( LINE_SIZE != 0 );
    return ( m_addr & ( ~( LINE_SIZE - 1 ) ) );
  }
  uint64_t lineOffset() const {
    assert( LINE_SIZE != 0 );
    return ( m_addr & ( LINE_SIZE - 1 ) );
  }
};



class DataAccess: public GenericAccess {
public:
  DataAccess( const MemoryAccessType mtype, const uint64_t addr, const unsigned size,
              const unsigned LSIZE ) :
    GenericAccess( mtype, addr, size, LSIZE ) {
    // ensure access fits within a cache line
    assert( lineOffset() + size <= LINE_SIZE );
  }
  void operator++( const int unused ) {
    m_addr++;
  }
  bool keepGoing() const {
    return m_addr < ( m_baseAddr + m_size );
  }
};

#endif /* MEMORYACCESS_HPP_ */
