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

#ifndef _IVDSIMSMPCACHE_H_
#define _IVDSIMSMPCACHE_H_

#include "HierarchicalCache.hpp"

#include "Counter.hpp"

#include "cachesim.hpp"

enum AccessResult_t {
  ACC_READHIT = 0x01000001, //
  ACC_READMISS_REM_SERV = 0x01000010, //
  ACC_READMISS_MEM_SERV = 0x01000100, //
  ACC_WRITEHIT = 0x10000001, //
  ACC_WRITEMISS = 0x10000010
};

class RemoteReadService {
public:
  bool isShared;
  bool providedData;

  RemoteReadService( bool shrd, bool prov ) {
    isShared = shrd;
    providedData = prov;
  }
};

class InvalidateReply {
public:
  bool nobodyHasThisLine;

  InvalidateReply( bool nhtl ) {
    nobodyHasThisLine = nhtl;
  }
};

class MemoryPermissions {
public:
  vector<MemoryAccessPermission> perms;

  MemoryPermissions( const unsigned blockSize ) {
    perms.assign( blockSize, NOTHING_OK );
  }
};

/** global map from sync objects to a vector clock snapshot. Lives in SimulatorThread.cpp */
extern map<uint64_t, vector<uint64_t> > g_vcOfSyncObject;

template<class State, class Addr_t = uint64_t>
class SMPCache : public CacheCallbacks<State> {

public:

  static const unsigned L1_HIT_LATENCY = 1;
  static const unsigned L2_HIT_LATENCY = 10;
  static const unsigned REMOTE_HIT_LATENCY = 15;
  static const unsigned L3_HIT_LATENCY = 35;
  //static const unsigned MEMORY_ACCESS_LATENCY = 120;
  static const unsigned MEMORY_ACCESS_LATENCY = 121;

  int CPUId;

  uint64_t timeInMemoryHierarchy;
  bool useDetStoreBuffers;
  bool storeBufferOverflowed;

  /** a deterministic accumulation of time spent in the memory hierarchy,
   * based only on hits to dirty lines in a store buffer (everything else
   * is assumed to be a shared cache hit). */
  uint64_t deterministicTimeInMemoryHierarchy;

  HierarchicalCache<State>* L1cache;
  HierarchicalCache<State>* L2cache;
  bool StoreBufferIsEmpty;

  /** L3, shared by all processors */
  HierarchicalCache<State>* L3cache;


  typedef typename vector<SMPCache<State, Addr_t> *>::const_iterator cache_iter_t;
  /** List of all the caches in the system. This points to the same vector for all caches */
  const vector<SMPCache<State, Addr_t>*>* allCaches;


  // counters for cache events

  Counter numReadHits;
  Counter numReadRemoteHits;
  Counter numReadMisses;
  Counter numWriteHits;
  Counter numWriteRemoteHits;
  Counter numWriteMisses;
  Counter numUpgradeMisses;
  Counter numTotalMemoryAccesses; /** only computed when we generate stats */
  Counter numL1Evictions;
  Counter numL2Evictions;
  Counter numDirtyDataEvictions;
  Counter numSyncSources;
  Counter numSyncTotalSinks;
  Counter numSyncSourcelessSinks;
  Counter numSyncUnmatchedSinks;

  // counters for RCDC events



  virtual ~SMPCache() {
    delete L1cache;
    delete L2cache;
  }

  SMPCache( int cpuid, int numCpus,
            HierarchicalCache<State>* l3,
            vector<SMPCache<State, Addr_t>*>* cacheVector,
            CacheConfiguration<State> l1config,
            bool useL2, CacheConfiguration<State> l2config ) :
        CPUId( cpuid ),
        timeInMemoryHierarchy( 0 ),
        useDetStoreBuffers( false ),
        storeBufferOverflowed( false ),
        deterministicTimeInMemoryHierarchy( 0 ),
        StoreBufferIsEmpty( true ),
        L3cache( l3 ),
        allCaches( cacheVector ),

#define COUNTER(name) name( Counter(cpuid,#name) )
        COUNTER(numReadHits),
        COUNTER(numReadRemoteHits),
        COUNTER(numReadMisses),
        COUNTER(numWriteHits),
        COUNTER(numWriteRemoteHits),
        COUNTER(numWriteMisses),
        COUNTER(numUpgradeMisses),
        COUNTER(numTotalMemoryAccesses),
        COUNTER(numL1Evictions),
        COUNTER(numL2Evictions),
        COUNTER(numDirtyDataEvictions),
        COUNTER(numSyncSources),
        COUNTER(numSyncTotalSinks),
        COUNTER(numSyncSourcelessSinks),
        COUNTER(numSyncUnmatchedSinks)
#undef COUNTER
  {
    L1cache = L2cache = NULL;

    l1config.callbacks = this;
    if ( useL2 ) {
      l2config.callbacks = this;
      L2cache = new HierarchicalCache<State>( l2config, l3 );
      L1cache = new HierarchicalCache<State>( l1config, L2cache );
    } else {
      L1cache = new HierarchicalCache<State>( l1config, l3 );
    }

  }

  typedef typename list<State*>::const_reverse_iterator set_riter_t;
  virtual State* eviction(const list<State*>& set, int level) {
    if ( !useDetStoreBuffers ) {
      return set.back();

    } else {

      // if we have an L2, L1 evictions don't trigger an overflow
      if ( L2cache && 1 == level ) {
        return set.back();
      }
      // At this point, either 1) there's no L2, or 2) we're dealing with an L2 eviction.
      // Either way there's a potential for SB overflow.

      // evict least-recently-used clean line
      set_riter_t li;
      for ( li = set.rbegin(); li != set.rend(); li++ ) {
        if ( (*li)->isClean() ) {
          return *li;
        }
      }
      // if we made it here, all lines in the set were dirty :-(
      storeBufferOverflowed = true;

      // have to clean evicted lines: in case they get re-filled into a SB for a read,
      // we don't want to erroneously think they're dirty
      set.back()->setClean();

      // might as well kick out the LRU line
      return set.back();
    }
  } // end eviction()

  /** Update counters that are lazily-computed. */
  virtual void finalizeCounters() {
    numTotalMemoryAccesses = numReadHits.get() + numReadRemoteHits.get() + numReadMisses.get()
        + numWriteHits.get() + numWriteRemoteHits.get() + numWriteMisses.get() + numUpgradeMisses.get();
  }

  /**
   * @param syncop whether this is a source or a sink in the HB graph
   * @param validSource whether sourceCpu is valid or not (e.g. the first lock acquire has no "source")
   * @param sourceCpu DEPRECATED if this is a sink, the cpu that was the source
   * @param syncObject the object being synchronized on */
  virtual void syncOp( SynchronizationOperationType syncop, bool validSource, unsigned sourceCpu,
                       uint64_t syncObject ) {
  } // end syncOp()


  /** Perform a data read specified by the given `access'. */
  virtual void read( const DataAccess& access ) {

    State* line = NULL;
    CacheResponse r = L1cache->search( access.addr(), line );

    if ( r != MISSED_TO_MEMORY ) {
      // we hit somewhere in our private cache(s), or a shared cache
      numReadHits++;
      switch ( r ) {
      case L1_HIT:
        timeInMemoryHierarchy += L1_HIT_LATENCY;
        // default det cache latency is an L1 hit, so it doesn't matter whether we hit to a dirty line or not
        deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        return;
      case L2_HIT: {
        timeInMemoryHierarchy += L2_HIT_LATENCY;
        if ( line->isDirty() ) {
          deterministicTimeInMemoryHierarchy += L2_HIT_LATENCY;
        } else {
          deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        }
        return;
      }
      case L3_HIT:
        timeInMemoryHierarchy += L3_HIT_LATENCY;
        // shared cache hits aren't det
        deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        return;
      default:
        assert(false);
      }
    }


    // we missed, so check remote caches for data
    RemoteReadService rrs = readRemoteAction( access );

    // remote hits and missing to memory aren't det
    deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;

    MESIState newMesiState = MESI_INVALID;
    if ( rrs.providedData == false ) {
      // No Valid Read-Reply: Need to get this data from Memory
      numReadMisses++;
      timeInMemoryHierarchy += MEMORY_ACCESS_LATENCY-1;

      /* NB: we always fetch from memory into Exclusive. */
      newMesiState = MESI_EXCLUSIVE;

    } else if ( rrs.isShared ) {
      newMesiState = MESI_SHARED;
      numReadRemoteHits++;
      timeInMemoryHierarchy += REMOTE_HIT_LATENCY;

    } else {
      //Valid Read-Reply From Modified/Exclusive
      newMesiState = MESI_SHARED;
      numReadRemoteHits++;
      timeInMemoryHierarchy += REMOTE_HIT_LATENCY;
    }
    assert( newMesiState != MESI_INVALID );

    // pull in the actual line
    L1cache->access( access.addr(), line );
    line->changeStateTo( newMesiState );

  } // end read()

  virtual RemoteReadService readRemoteAction( const DataAccess& access ) {
    cache_iter_t cacheIter;
    for ( cacheIter = allCaches->begin(); cacheIter != allCaches->end(); cacheIter++ ) {
      SMPCache<State, Addr_t> *otherCache = *cacheIter;
      if ( otherCache->CPUId == this->CPUId ) {
        continue;
      }

      State* otherLine = NULL;
      CacheResponse r = otherCache->L1cache->search( access.addr(), otherLine );
      if ( r == MISSED_TO_MEMORY ) {
        continue; // not found in this cache
      }
      // found in a remote cache!

      switch ( otherLine->getState() ) {
      case MESI_EXCLUSIVE:
      case MESI_MODIFIED:
        otherLine->changeStateTo( MESI_SHARED );
        return RemoteReadService( false, true );
      case MESI_SHARED:
        // everyone else will be in Shared state as well, so return now
        return RemoteReadService( true, false );
      case MESI_INVALID:
        // keep searching for other copies
        break;
      default:
        assert(false);
      }

    } // done with other caches

    // this happens if everyone was MESI_INVALID
    return RemoteReadService( false, false );
  } // end readRemoteAction()


  /** Make a write request.
   * @param access the memory access being made
   * @param doStoreBufferAccess whether to use the det store buffer for this write */
  virtual void write( const DataAccess& access, bool doStoreBufferAccess ) {
    State* myLine = NULL;
    CacheResponse r = L1cache->search( access.addr(), myLine );

    if ( r != MISSED_TO_MEMORY ) {

      switch (r) {
      case L1_HIT:
        timeInMemoryHierarchy += L1_HIT_LATENCY;
        // default det cache latency is an L1 hit, so it doesn't matter whether we hit to a dirty line or not
        deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        break;
      case L2_HIT: {
        timeInMemoryHierarchy += L2_HIT_LATENCY;
        if ( myLine->isDirty() ) {
          deterministicTimeInMemoryHierarchy += L2_HIT_LATENCY;
        } else {
          deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        }
        break;
      }
      case L3_HIT:
        timeInMemoryHierarchy += L3_HIT_LATENCY;
        // shared cache hits aren't det
        deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;
        break;
      default:
        assert(false);
      }

      // we either hit somewhere in our private cache(s) or we hit in the shared cache,
      // but we may not have write permissions
      L1cache->access( access.addr(), myLine );

      switch ( myLine->getState() ) {
      case MESI_SHARED: // upgrade miss
        numUpgradeMisses++;
        writeRemoteAction( access ); // invalidate other copies
        timeInMemoryHierarchy += REMOTE_HIT_LATENCY;

        myLine->changeStateTo( MESI_MODIFIED );
        if ( useDetStoreBuffers && doStoreBufferAccess ) {
          myLine->setDirty();
          StoreBufferIsEmpty = false;
        }
        return;
      case MESI_EXCLUSIVE: // write hit
      case MESI_MODIFIED:
        numWriteHits++;

        // TODO: fix this duplication from the MESI_SHARED case
        myLine->changeStateTo( MESI_MODIFIED );
        if ( useDetStoreBuffers && doStoreBufferAccess ) {
          myLine->setDirty();
          StoreBufferIsEmpty = false;
        }
        return;
      default:
        assert(false);
      }

    }

    // we didn't have the line at all - need to check for remote copies

    InvalidateReply inv_ack = writeRemoteAction( access );

    // remote hits and missing to memory aren't det
    deterministicTimeInMemoryHierarchy += L1_HIT_LATENCY;

    if ( inv_ack.nobodyHasThisLine ) {
      numWriteMisses++;
      timeInMemoryHierarchy += MEMORY_ACCESS_LATENCY;
    } else {
      numWriteRemoteHits++;
      timeInMemoryHierarchy += REMOTE_HIT_LATENCY;
    }

    L1cache->access( access.addr(), myLine );

    myLine->changeStateTo( MESI_MODIFIED );
    if ( useDetStoreBuffers && doStoreBufferAccess ) {
      myLine->setDirty();
      StoreBufferIsEmpty = false;
    }

  } // end write()

  virtual InvalidateReply writeRemoteAction( const DataAccess& access ) {
    cache_iter_t cacheIter;

    bool noOtherCachesHaveLine = true;
    for ( cacheIter = allCaches->begin(); cacheIter != allCaches->end(); cacheIter++ ) {
      SMPCache<State, Addr_t> *otherCache = *cacheIter;
      if ( otherCache->CPUId == this->CPUId ) {
        continue;
      }

      State* otherLine = NULL;
      CacheResponse r = otherCache->L1cache->search( access.addr(), otherLine );
      if ( r == MISSED_TO_MEMORY ) { // not found in this cache
        continue;
      }

      switch ( otherLine->getState() ) {
      case MESI_MODIFIED:
      case MESI_EXCLUSIVE:
      case MESI_SHARED:
        otherLine->invalidate();
        noOtherCachesHaveLine = false;
        // have to keep searching to find all Shared copies
        break;
      case MESI_INVALID:
        // keep searching for more copies
        break;
      default:
        assert(false);
      }
    } // done with other caches

    return InvalidateReply( noOtherCachesHaveLine );
  } // end writeRemoteAction()

};

#endif
