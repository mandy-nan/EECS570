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

#include "HierarchicalCache.hpp"

static HierarchicalCache<VILine>* l1 = NULL;
static HierarchicalCache<VILine>* l2 = NULL;

static const int L1_ASSOC = 2;
static const int BLOCK_SIZE = 4;
static const int L1_CACHE_SIZE = 16; // 4 blocks, 2 sets
static const int L2_ASSOC = 2;
static const int L2_CACHE_SIZE = 8; // 4 blocks, 2 sets

static struct CacheConfiguration<VILine> l1config;
static struct CacheConfiguration<VILine> l2config;

template<class Line>
class Callbacks : public CacheCallbacks<Line> {
  virtual Line* eviction(const list<Line*>& set, int level) {
    return set.back();
  }
};

struct HierarchicalCacheL1Bookends {
  // runs before each test case
  HierarchicalCacheL1Bookends() {
    l1config.cacheSize = L1_CACHE_SIZE;
    l1config.blockSize = BLOCK_SIZE;
    l1config.assoc = L1_ASSOC;
    l1config.callbacks = new Callbacks<VILine>();
    l1 = new HierarchicalCache<VILine>( l1config, NULL );
  }
  // runs after each test case
  ~HierarchicalCacheL1Bookends() {
    delete l1config.callbacks;
    delete l1;
    l1 = l2 = NULL;
  }
};

struct HierarchicalCacheL2Bookends {
  // runs before each test case
  HierarchicalCacheL2Bookends() {
    l1config.blockSize = BLOCK_SIZE;
    l1config.cacheSize = L1_CACHE_SIZE;
    l1config.assoc = L1_ASSOC;
    l1config.callbacks = new Callbacks<VILine>();

    l2config.blockSize = BLOCK_SIZE;
    l2config.cacheSize = L2_CACHE_SIZE;
    l2config.assoc = L2_ASSOC;
    l2config.callbacks = new Callbacks<VILine>();

    l2 = new HierarchicalCache<VILine>( l2config, NULL );
    l1 = new HierarchicalCache<VILine>( l1config, l2 );
  }
  // runs after each test case
  ~HierarchicalCacheL2Bookends() {
    delete l1config.callbacks;
    delete l2config.callbacks;
    delete l1;
    delete l2;
    l1 = l2 = NULL;
  }
};


BOOST_FIXTURE_TEST_SUITE( HierCacheL1, HierarchicalCacheL1Bookends )

BOOST_AUTO_TEST_CASE( l1access ) {

  CacheResponse r;
  VILine* ignore;

  // bring 0d into the cache
  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );

  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, L1_HIT );

  // fetch 16d and 32d which map to set 0; this evicts 0d
  for ( int i = 1; i <= l1config.assoc; i++ ) {
    r = l1->access( l1config.cacheSize*i, ignore );
    BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );
  }

  // fetch 0d again, from memory
  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_FIXTURE_TEST_SUITE( HierCacheL2, HierarchicalCacheL2Bookends )

BOOST_AUTO_TEST_CASE( l2access ) {

  CacheResponse r;
  VILine* ignore;

  // bring 0d into the cache
  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );

  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, L1_HIT );

  // fetch 16d and 32d which map to set 0; this evicts 0d to L2
  for ( int i = 1; i <= l1config.assoc; i++ ) {
    r = l1->access( l1config.cacheSize*i, ignore );
    BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );
  }

  // fetch 0d again, from L2
  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, L2_HIT );

  r = l1->access( 0, ignore );
  BOOST_CHECK_EQUAL( r, L1_HIT );
}

BOOST_AUTO_TEST_CASE( l3TagBug ) {
  CacheResponse r;
  VILine* ignore;

  // bring 4d into the cache
  r = l1->access( 4, ignore );
  BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );

  // fetch 20d and 36d which map to set 1; this evicts 4d to the L2
  for ( int i = 1; i <= l1config.assoc; i++ ) {
    r = l1->access( 4 + (l1config.cacheSize*i), ignore );
    BOOST_CHECK_EQUAL( r, MISSED_TO_MEMORY );
  }

  // fetch 4d again, from the L2
  r = l1->access( 4, ignore );
  BOOST_CHECK_EQUAL( r, L2_HIT );
}

BOOST_AUTO_TEST_CASE( l2randomAccessees ) {

  VILine* ignore;

  for ( int i = 0; i < 100000; i++ ) {
    l1->access( rand(), ignore );
  }

}

BOOST_AUTO_TEST_SUITE_END()
