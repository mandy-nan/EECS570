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
 * A simple cache object that can be easily chained
 * together into a hierarchy.
 */

#ifndef HIERARCHICALCACHE_HPP_
#define HIERARCHICALCACHE_HPP_

#include <vector>
#include <list>
#include <deque>
#include <stdint.h>
#include <iostream>

using namespace std;

template<class Line>
class CacheCallbacks {
public:
  /** Called whenever a line needs to be evicted.
   * @param set the set from which we need to evict something; the list is ordered
   * from MRU (front) to LRU (back).
   * @return the line to evict */
  virtual Line* eviction(const list<Line*>& set, int level) = 0;
};

template<class Line>
class CacheConfiguration {
public:
  int cacheSize;
  int assoc;
  int blockSize;
  CacheCallbacks<Line>* callbacks;
};

class VILine {
protected:
  bool m_valid;
  uint64_t m_tag;

public:
  VILine() : m_valid(false), m_tag(0) {}
  VILine(uint64_t tag) : m_valid(true), m_tag(tag) {}

  bool valid() const { return m_valid; }
  bool invalid() const { return !m_valid; }
  void invalidate() { m_valid = false; }
  uint64_t tag() const { return m_tag; }
};

enum MESIState { MESI_MODIFIED = 1, MESI_EXCLUSIVE, MESI_SHARED, MESI_INVALID };

class MESILine : public VILine {
protected:
  MESIState m_state;

public:
  MESILine() : VILine(), m_state(MESI_INVALID) {}
  MESILine(uint64_t tag) : VILine(tag), m_state(MESI_INVALID) {}

  MESIState getState() { return m_state; }
  void changeStateTo(MESIState s) { m_state = s; }
};

class RCDCLine : public MESILine {
protected:
  bool m_dirty;

public:
  RCDCLine() : MESILine(), m_dirty(false) {}
  RCDCLine(uint64_t tag) : MESILine(tag), m_dirty(false) {}

  void setDirty() { m_dirty = true; }
  void setClean() { m_dirty = false; }
  bool isDirty() { return m_dirty; }
  bool isClean() { return !m_dirty; }
};



enum CacheResponse { L1_HIT=1, L2_HIT, L3_HIT, MISSED_TO_MEMORY };

template<class Line = VILine>
class HierarchicalCache {
protected:
  /** 1 is the L1, 2 is L2, etc. */
  int m_levelInHierarchy;

  /** associativity */
  unsigned m_assoc;

  /** log_2(block size) */
  uint32_t m_blockOffsetBits;
  /** log_2(num sets) */
  uint32_t m_indexBits;
  /** mask used to clear out the tag bits */
  uint64_t m_indexMask;

  typedef typename list<Line*>::iterator line_iter_t;
  typedef typename list<Line*>::const_iterator const_line_iter_t;
  typedef typename vector<Line*>::iterator all_line_iter_t;
  typedef typename vector< list<Line*> >::iterator set_iter_t;

  vector< list<Line*> > m_sets;

  CacheCallbacks<Line>* m_callbacks;

public:

  /** The next higher-level cache in the hierarchy. Can be shared by
   * multiple lower-level caches. */
  HierarchicalCache<Line>* m_nextCache;


// CODE

protected:

  bool isPowerOf2(uint64_t n) const {
    // thank you, Hacker's Delight!
    return 0 == ( n & (n-1) );
  }

  uint64_t floorLog2(uint64_t n) const {
    // thank you, Hacker's Delight!
    if (n == 0)
      return -1;

    int pos = 0;
    if (n >= 1LL<<32) { n >>= 32; pos += 32; }
    if (n >= 1  <<16) { n >>= 16; pos += 16; }
    if (n >= 1  << 8) { n >>=  8; pos +=  8; }
    if (n >= 1  << 4) { n >>=  4; pos +=  4; }
    if (n >= 1  << 2) { n >>=  2; pos +=  2; }
    if (n >= 1  << 1) {           pos +=  1; }
    return pos;
  }

  /** Set this cache's level in the hierarchy, and update all higher
   * levels of the hierarchy as well. */
  void setLevel(int level) {
    m_levelInHierarchy = level;
    if ( m_nextCache ) {
      m_nextCache->setLevel( level + 1 );
    }
  }

  /** Return the cache index for the given address */
  uint64_t index(uint64_t address) const {
    return (address >> m_blockOffsetBits) & m_indexMask;
  }

  uint64_t tag(uint64_t address) const {
    return address >> (m_blockOffsetBits + m_indexBits);
  }

  /** Put the incoming line into this cache.
   * @param incoming the line being evicted from a lower-level cache
   * @param blockAddress address of the first byte in incoming */
  void evictedFromLowerCache(Line* incoming, uint64_t blockAddress) {
    if ( incoming->invalid() ) {
      // ignore the eviction of invalid lines
      delete incoming;
      return;
    }

    list<Line*>& set = m_sets.at( index(blockAddress) );

    if(!m_nextCache)
	    cout << "set size before remove for the first eviction: " << set.size() << endl; 

    Line* toEvict = m_callbacks->eviction( set, m_levelInHierarchy );

    if(!m_nextCache)
	    cout << "set size before remove for the first eviction: " << set.size() << endl; 

    set.remove( toEvict );

    if(!m_nextCache)
	    cout << "set size after remove for the first eviction: " << set.size() << endl; 
	    //cout << "set size after remove the first eviction: " << set.size() << endl; 
    // NB: don't let eviction handler see the incoming line
    set.push_front( incoming );

    if(!m_nextCache){
	    cout << "set size after add the first eviction: " << set.size() << endl; 
	    assert(set.size()==m_assoc);
    }

    if ( m_nextCache ) {
      uint64_t lruBlockAddress = (toEvict->tag() << m_indexBits) + index(blockAddress);
      m_nextCache->evictedFromLowerCache( toEvict, lruBlockAddress );
    } else {
//New Mandy****************************************************************
    int index_num_max = 1 << (m_indexBits+1) -1;
    int index_num = index(blockAddress);
    index_num = (index_num + 1)%index_num_max;
    set = m_sets.at(index_num);

    toEvict = m_callbacks->eviction( set, m_levelInHierarchy );
    set.remove( toEvict );
    set.push_front( incoming );
    cout << "set size after add the second eviction: " << set.size() << endl; 
    assert(set.size()==m_assoc);
    delete toEvict;
    //cout << "set size after remove the second eviction: " << set.size() << endl; 
    //cout << "set size after add the second eviction: " << set.size() << endl; 
    //delete toEvict_n;
//New Mandy****************************************************************
    }
  }

  /** access the line containing the given address, bringing it into the L1 cache if it
   * is not already present there
   * @param address the memory address being accessed
   * @param l1Line output parameter that points to the line containing address
   * (it resides in this core's L1 cache)
   * @param hit an output parameter that points to the Line that was hit in some higher-level cache
   * @return the level of the cache hierarchy where the hit occurred */
  CacheResponse access(const uint64_t address, Line*& l1Line, Line*& higherLevelHit) {
    list<Line*>& set = m_sets.at( index(address) );
    if(set.size() != m_assoc){
	cout << "the faulty set_size is: " << set.size() << endl;
	cout << "m_assoc is: " << m_assoc << endl;
    }
    assert( set.size() == m_assoc );

    // search this cache
    line_iter_t it;
    for ( it = set.begin(); it != set.end(); it++ ) {
      Line* line = *it;
      if ( line->valid() && line->tag() == tag(address) ) {
        // hit! move this line to the mru spot
        if ( 1 == m_levelInHierarchy ) {
          if ( it != set.begin() ) {
            set.erase( it );
            set.push_front( line );
          }
          l1Line = set.front();

        } else { // L2+ cache
          set.erase( it );
          set.push_back( new Line() );
          higherLevelHit = line; // allow line to be sent to lower-level cache
        }

        return (CacheResponse) m_levelInHierarchy;
      }
    }

    // if we made it here, we missed in this cache

    CacheResponse response = MISSED_TO_MEMORY;
    Line* higherLevelCacheHit = NULL;
    if ( m_nextCache ) {
      response = m_nextCache->access( address, l1Line,
                                      (1 == m_levelInHierarchy) ? higherLevelCacheHit : higherLevelHit );
      // response should have come from deeper in the hierarchy
      assert( response > m_levelInHierarchy );
    }

    // only L1 caches have to deal with evictions here;
    // other levels are handled via evictedFromLowerCache()
    if ( 1 != m_levelInHierarchy ) return response;


    switch ( response ) {
    case MISSED_TO_MEMORY:
      // bring line from memory straight into L1
      l1Line = new Line( tag(address) );
      break;
    case L1_HIT:
    case L2_HIT:
    case L3_HIT:
      // bring in hit line from higher-level cache
      assert( higherLevelCacheHit != NULL );
      l1Line = higherLevelCacheHit;
      break;

    default:
      assert(false);
    }

    // evict a line (possibly to next-level cache)
    Line* toEvict = m_callbacks->eviction( set, m_levelInHierarchy );
    set.remove( toEvict );

    // NB: only push in the incoming line *after* we've evicted something
    set.push_front( l1Line );

    if ( m_nextCache ) {
      uint64_t blockAddress = (toEvict->tag() << m_indexBits) + index(address);
      m_nextCache->evictedFromLowerCache( toEvict, blockAddress );
    } else {
      delete toEvict;
    }

    return response;

  } // end access()


public:

  /** Construct a component of the cache hierarchy.  The components must be constructed from highest -> lowest.
   * @param thisConfig geometry and event handlers for this cache
   * @param nextCache the next higher cache in the hierarchy */
  HierarchicalCache( const CacheConfiguration<Line>& thisConfig,
                     HierarchicalCache<Line>* nextCache ) :
        m_levelInHierarchy( 1 ),
        m_assoc( thisConfig.assoc ),
        m_callbacks( thisConfig.callbacks ),
        m_nextCache( nextCache )
  {
    // set the level of the next-higher-up cache based on my level
    if ( m_nextCache ) {
      m_nextCache->setLevel( m_levelInHierarchy + 1 );
    }

    // construct the cache itself

    assert( isPowerOf2(thisConfig.blockSize) );
    assert( isPowerOf2(thisConfig.assoc) );
    assert( isPowerOf2(thisConfig.cacheSize) );

    int numSets = thisConfig.cacheSize / (thisConfig.blockSize * thisConfig.assoc);
    assert( isPowerOf2(numSets) );
    assert( numSets * thisConfig.blockSize * thisConfig.assoc == thisConfig.cacheSize );
    m_indexMask = numSets - 1;
    m_indexBits = floorLog2( numSets );
    m_blockOffsetBits = floorLog2( thisConfig.blockSize );

    for ( int i = 0; i < numSets; i++ ) {
      list<Line*> set;
      for ( int j = 0; j < thisConfig.assoc; j++ ) {
        Line* line = new Line();
        set.push_back( line );
      }
      m_sets.push_back( set );
    }

  } // end ctor

  virtual ~HierarchicalCache() {
    for ( set_iter_t s = m_sets.begin(); s != m_sets.end(); s++ ) {
      for ( line_iter_t l = s->begin(); l != s->end(); l++ ) {
        delete *l;
      }
    }
  }

  /** access the line containing the given address, bringing it into the L1 cache if it
   * is not already present there
   * @param address the memory address being accessed
   * @param l1Line output parameter that points to the line containing address
   * (it resides in this core's L1 cache)
   * @return the level of the cache hierarchy where the hit occurred */
  CacheResponse access(const uint64_t address, Line*& l1Line) {
    Line* ignore;
    return access( address, l1Line, ignore );
  }

  /** Returns where the given address exists in this core's cache hierarchy,
   * without modifying any cache state.
   * @param line output parameter that points to the line containing address (somewhere in this
   * core's hierarchy), iff a hit occurs */
  CacheResponse search(const uint64_t address, Line*& myLine) const {
    const list<Line*>& set = m_sets.at( index(address) );

    // search this cache
    const_line_iter_t it;
    for ( it = set.begin(); it != set.end(); it++ ) {
      Line* l = *it;
      if ( l->valid() && l->tag() == tag(address) ) {
        // hit!
        myLine = l;
        return (CacheResponse) m_levelInHierarchy;
      }
    }

    // at this point, we missed in this cache

    if ( m_nextCache ) {
      return m_nextCache->search( address, myLine );
    }

    // we're the last cache in the hierarchy
    // line is unmodified
    return MISSED_TO_MEMORY;

  } // end search()

  /** Calls the given function once on each line in this cache. Lines
   * are traversed in no particular order. */
  void visitAllLines(void (*fun)(Line*)) {
    for ( set_iter_t s = m_sets.begin(); s != m_sets.end(); s++ ) {
      for ( line_iter_t l = s->begin(); l != s->end(); l++ ) {
        fun( *l );
      }
    }
  }


};

#endif /* HIERARCHICALCACHE_HPP_ */
