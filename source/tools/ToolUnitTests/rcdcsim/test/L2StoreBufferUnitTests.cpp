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

static SMPCache<RCDCLine, uint64_t>* nondetCore = NULL;
static HierarchicalCache<RCDCLine>* nondetL3 = NULL;

static SMPCache<RCDCLine, uint64_t>* rcdcCore = NULL;
static HierarchicalCache<RCDCLine>* rcdcL3 = NULL;

static const int BLOCK_SIZE = 64;
static const int L1_ASSOC = 8;
static const int L1_CACHE_SIZE = 1<<15;
static const int L2_ASSOC = 8;
static const int L2_CACHE_SIZE = 1<<18;
static const int L3_ASSOC = 16;
static const int L3_CACHE_SIZE = 1<<23;

static struct CacheConfiguration<RCDCLine> l1config;
static struct CacheConfiguration<RCDCLine> l2config;
static struct CacheConfiguration<RCDCLine> l3config;

template<class Line = RCDCLine>
class LRUEvictionHandler : public CacheCallbacks<Line> {
public:
  Line* eviction(const list<Line*>& set, int level) {
    return set.back();
  }
};
static LRUEvictionHandler<> lru;

static vector< SMPCache<RCDCLine, uint64_t>* > nondetVec;
static vector< SMPCache<RCDCLine, uint64_t>* > rcdcVec;

struct L2StoreBufferBookends {
  // runs before each test case
  L2StoreBufferBookends() {
    l1config.blockSize = l2config.blockSize = l3config.blockSize = BLOCK_SIZE;

    l1config.cacheSize = L1_CACHE_SIZE;
    l1config.assoc = L1_ASSOC;
    l1config.callbacks = NULL;

    l2config.cacheSize = L2_CACHE_SIZE;
    l2config.assoc = L2_ASSOC;
    l2config.callbacks = NULL;

    l3config.cacheSize = L3_CACHE_SIZE;
    l3config.assoc = L3_ASSOC;
    l3config.callbacks = &lru;

    nondetL3 = new HierarchicalCache<RCDCLine>( l3config, NULL );
    nondetCore = new SMPCache<RCDCLine, uint64_t>( 0, 1, rcdcL3, &nondetVec, l1config, true, l2config );

    // RCDC stuff
    rcdcL3 = new HierarchicalCache<RCDCLine>( l3config, NULL );
    rcdcCore = new SMPCache<RCDCLine, uint64_t>( 0, 1, rcdcL3, &rcdcVec, l1config, true, l2config );
    rcdcCore->useDetStoreBuffers = true;
  }
  // runs after each test case
  ~L2StoreBufferBookends() {
    delete rcdcCore;
    delete rcdcL3;

    delete nondetCore;
    delete nondetL3;

    nondetVec.clear();
    rcdcVec.clear();
  }
};


BOOST_FIXTURE_TEST_SUITE( L2StoreBuffer, L2StoreBufferBookends )

BOOST_AUTO_TEST_CASE( l2StoreBufferBakeoff ) {

}

BOOST_AUTO_TEST_SUITE_END()
