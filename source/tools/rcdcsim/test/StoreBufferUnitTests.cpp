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

#include <boost/test/unit_test.hpp>

#include "SMPCache.hpp"

static SMPCache<RCDCLine, uint64_t>* core = NULL;

static const int L1_ASSOC = 2;
static const int BLOCK_SIZE = 4;
static const int L1_CACHE_SIZE = 16; // 4 blocks, 2 sets
static const int L2_ASSOC = 2;
static const int L2_CACHE_SIZE = 16; // 4 blocks, 2 sets

static struct CacheConfiguration<RCDCLine> l1config;


struct StoreBufferBookends {
  // runs before each test case
  StoreBufferBookends() {
    l1config.cacheSize = L1_CACHE_SIZE;
    l1config.blockSize = BLOCK_SIZE;
    l1config.assoc = L1_ASSOC;
    l1config.callbacks = NULL;

    vector< SMPCache<RCDCLine, uint64_t>* > vec;
    core = new SMPCache<RCDCLine, uint64_t>( 0, 1, NULL, &vec, l1config, false, l1config );
    core->useDetStoreBuffers = true;
  }
  // runs after each test case
  ~StoreBufferBookends() {
    delete core;
  }
};


BOOST_FIXTURE_TEST_SUITE( StoreBuffer, StoreBufferBookends )

BOOST_AUTO_TEST_CASE( storeBufferOverflow ) {

  BOOST_CHECK( !core->storeBufferOverflowed );

  // fill up set with dirty lines
  int i;
  for ( i = 0; i < l1config.assoc; i++ ) {
    core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*i, 1, l1config.blockSize ), true );
    BOOST_CHECK( !core->storeBufferOverflowed );
  }

  core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*i, 1, l1config.blockSize ), true );
  BOOST_CHECK( core->storeBufferOverflowed );
}

BOOST_AUTO_TEST_CASE( evictingCleanLines ) {

  BOOST_CHECK( !core->storeBufferOverflowed );

  // fill up all but one way of a set with dirty lines
  int i;
  for ( i = 0; i < l1config.assoc-1; i++ ) {
    core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*i, 1, l1config.blockSize ), true );
    BOOST_CHECK( !core->storeBufferOverflowed );
  }

  // we just cycle data through the clean way
  for ( int j = 0; j < 100; j++ ) {
    core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*j, 1, l1config.blockSize ), false );
    BOOST_CHECK( !core->storeBufferOverflowed );
  }

  // finally, trigger an overflow
  core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*i, 1, l1config.blockSize ), true );
  BOOST_CHECK( !core->storeBufferOverflowed );
  i++;
  core->write( DataAccess( WRITE_ACCESS, l1config.cacheSize*i, 1, l1config.blockSize ), true );
  BOOST_CHECK( core->storeBufferOverflowed );
}

BOOST_AUTO_TEST_SUITE_END()

