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

#ifndef _MULTICACHESIM_H_
#define _MULTICACHESIM_H_

#include "SMPCache.hpp"

#include "cachesim.hpp"

void unblockAllApplicationThreads();

template<class Line = RCDCLine, class Addr_t = uint64_t>
class MultiCacheSimulator : public CacheCallbacks<Line> {
public:
  typedef SMPCache<Line, uint64_t> cache_t;
  typedef typename vector<cache_t*>::iterator cache_iter_t;
  vector<cache_t*> m_allCaches;

  const unsigned NUM_CORES;

  bool m_simulateTSO;
  bool m_simulateHB;
  unsigned m_quantumSize;
  bool m_smartQuantumBuilding;


private:
  const int LINE_SIZE;

  HierarchicalCache<Line>* m_l3cache;

  /** HACK: so that we know how many cores to wait for at a quantum boundary */
  unsigned m_liveThreads;
  vector<uint64_t> m_insnCounts;
  vector<uint64_t> m_workCounts;
  vector<bool> m_blocked;
  vector<bool> m_stalledAtQuantumBoundary;
  vector<bool> m_waitingForCausality;
  /** used for computing average insns per quantum */
  uint64_t m_sumOfInsnsPerQuantum;
  /** used for computing average quantum latency (in cycles) */
  uint64_t m_sumOfCyclesPerQuantum;
  bool commitThisRound;

  /** Tells the quantum round in which a sync source event occurred */
  map<uint64_t,uint64_t> m_roundOfSyncSource;
  Counter Runtime;
  Counter TotalQuantumImbalance;
  Counter QuantumRounds;
  Counter TotalQuanta;
  Counter QuantumRoundCommits;
  Counter AverageInsnsPerQuantum;
  Counter AverageCyclesPerQuantum;
  Counter SyncInducedRoundBoundaries;
  Counter StoreBufferOverflows;
  Counter InsnCountInducedRoundBoundaries;

public:
  MultiCacheSimulator( int numCaches, CacheConfiguration<Line> l1config,
                       bool useL2, CacheConfiguration<Line> l2config,
                       bool useL3, CacheConfiguration<Line> l3config ) :
                         NUM_CORES( numCaches ),
                         m_simulateTSO( false ),
                         m_simulateHB( false ),
                         m_quantumSize( 0 ),
                         m_smartQuantumBuilding( false ),

                         LINE_SIZE(l1config.blockSize),
                         m_sumOfInsnsPerQuantum( 0 ),
                         m_sumOfCyclesPerQuantum( 0 ),
                         commitThisRound( false ),

#define COUNTER(name) name( Counter(0,#name) )
                         COUNTER(Runtime),
                         COUNTER(TotalQuantumImbalance),
                         COUNTER(QuantumRounds),
                         COUNTER(TotalQuanta),
                         COUNTER(QuantumRoundCommits),
                         COUNTER(AverageInsnsPerQuantum),
                         COUNTER(AverageCyclesPerQuantum),
                         COUNTER(SyncInducedRoundBoundaries),
                         COUNTER(StoreBufferOverflows),
                         COUNTER(InsnCountInducedRoundBoundaries)
#undef COUNTER
  {

    m_insnCounts.assign( NUM_CORES, 0 );
    m_workCounts.assign( NUM_CORES, 0 );
    m_blocked.assign( NUM_CORES, false );
    m_stalledAtQuantumBoundary.assign( NUM_CORES, false );
    m_waitingForCausality.assign( NUM_CORES, false );

    l3config.callbacks = this;

    m_l3cache = NULL;
    if ( useL3 ) {
      m_l3cache = new HierarchicalCache<Line>( l3config, NULL );
    }

    for ( unsigned i = 0; i < NUM_CORES; i++ ) {
      cache_t *newcache = new cache_t( i, NUM_CORES, m_l3cache,
                                       &m_allCaches, l1config, useL2, l2config );
      m_allCaches.push_back( newcache );
    }
  }

  ~MultiCacheSimulator() {
    cache_iter_t cacheIter = m_allCaches.begin();
    for ( ; cacheIter != m_allCaches.end(); cacheIter++ ) {
      delete ( *cacheIter );
    }

    delete m_l3cache;
  }

  /** handles L3 evictions using plain LRU */
  virtual Line* eviction(const list<Line*>& set, int level) {
    return set.back();
  }

  /** map from thread id to cpu */
  unsigned cpuOfTid( unsigned tid ) {
    // NB: we have a very simple mapping of threads onto caches
    return tid % m_allCaches.size();
  }

  cache_t *getCache( unsigned tid ) {
    cache_t *c = m_allCaches.at( cpuOfTid( tid ) );
    assert( NULL != c );
    return c;
  }

  void dumpStats( ostream & os, const string & prefix, const string & suffix ) {
    cache_iter_t cacheIter = m_allCaches.begin();
    for ( ; cacheIter != m_allCaches.end(); cacheIter++ ) {
      ( *cacheIter )->finalizeCounters();
    }
    finishQuantumRound();

    if ( TotalQuanta.get() != 0 ) {
      AverageInsnsPerQuantum.set( m_sumOfInsnsPerQuantum / TotalQuanta.get() );
      AverageCyclesPerQuantum.set( m_sumOfCyclesPerQuantum / TotalQuanta.get() );
    }

    // the Counter class keeps track of all its instances, so we only need to dump once
    Counter::dumpCounters( os, prefix, suffix );
  }

  void cacheRead( const int tid, const Addr_t addr, const unsigned size,
                  bool doStoreBufferAccess = true ) {
    cacheAccess( tid, false, addr, size, doStoreBufferAccess );
  }

  void cacheWrite( int tid, Addr_t addr, unsigned size, bool doStoreBufferAccess = true ) {
    cacheAccess( tid, true, addr, size, doStoreBufferAccess );
  }

  void cacheAccess( const int tid, const bool write, const Addr_t addr,
                    const unsigned size, bool doStoreBufferAccess ) {
    assert( !stalledAtQuantumBoundary(tid) );
    cache_t* c = getCache( tid );

    for ( Addr_t a = addr, remainingSize = size; remainingSize > 0; ) {
      Addr_t data_bytesFromStartOfLine = a & ( LINE_SIZE - 1 );
      Addr_t data_maxSizeAccessWithinThisLine = LINE_SIZE - data_bytesFromStartOfLine;

      // data access
      Addr_t accessSize = min( remainingSize, data_maxSizeAccessWithinThisLine );
      if ( write ) {
        c->write( DataAccess( WRITE_ACCESS, a, accessSize, LINE_SIZE ), doStoreBufferAccess );
      } else {
        c->read( DataAccess( READ_ACCESS, a, accessSize, LINE_SIZE ) );
      }

      a += accessSize;
      remainingSize -= accessSize;
    }

    if ( m_simulateHB || m_simulateTSO ) {
      unsigned cpuid = cpuOfTid( tid );
      if ( m_smartQuantumBuilding ) {
        // NB: we increment work here based on the memory access that just happened, but
        // we only check for quantum ending at basic block boundaries
        m_workCounts.at( cpuid ) += c->deterministicTimeInMemoryHierarchy;
        c->deterministicTimeInMemoryHierarchy = 0; // reset for next access
      }

      if ( c->storeBufferOverflowed ) {
        m_stalledAtQuantumBoundary.at( cpuid ) = true;
        StoreBufferOverflows++;
        TotalQuanta++;
        commitThisRound = true;

        // check to see if I'm the last one in
        if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
      }
    }

  } // end cacheAccess()

  /**
   * @param tid the thread performing the sync op
   * @param op whether this is a source or a sink in the HB graph
   * @param validSource whether sourceTid is valid or not (e.g. the first lock acquire has no "source")
   * @param sourceTid if this is a sink, the thread that was the source
   * @param syncObject the object being synchronized on */
  void syncOp( int tid, SynchronizationOperationType op, bool validSource,
               unsigned sourceTid, uint64_t syncObject ) {
    assert( !stalledAtQuantumBoundary(tid) );
    getCache( tid )->syncOp( op, validSource, cpuOfTid( sourceTid ), syncObject );

    if ( m_simulateTSO && SYNC_SINK == op ) {
      m_stalledAtQuantumBoundary.at( cpuOfTid(tid) ) = true;
      SyncInducedRoundBoundaries++;
      TotalQuanta++;
      commitThisRound = true;

      // check to see if I'm the last one in
      if ( weAreDoneWithQuantumRound() ) finishQuantumRound();

    } else if ( m_simulateHB ) {
      switch ( op ) {
      case SYNC_SINK: {
        map<uint64_t,uint64_t>::iterator i = m_roundOfSyncSource.find( syncObject );
        if ( i != m_roundOfSyncSource.end() ) {
          uint64_t releaseRound = i->second;
          assert( releaseRound <= QuantumRounds.get() );
          if ( releaseRound == QuantumRounds.get() ) {
            // release occurred in this round: have to stall
            m_stalledAtQuantumBoundary.at( cpuOfTid(tid) ) = true;
            SyncInducedRoundBoundaries++;
            TotalQuanta++;
            commitThisRound = true;

            // check to see if I'm the last one in
            if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
          }
        }
      }
        break;
      case SYNC_SOURCE:
        m_roundOfSyncSource[ syncObject ] = QuantumRounds.get();
        break;
      default:
        assert(false);
      }
    }
  } // end syncOp()

  void basicBlock( int tid, unsigned insnCount ) {
    unsigned cpuid = cpuOfTid( tid );
    m_insnCounts.at( cpuid ) += insnCount;
    m_workCounts.at( cpuid ) += insnCount;

    if ( !(m_simulateHB || m_simulateTSO) ) {
      return;
    }
    assert( !stalledAtQuantumBoundary(tid) );

    if ( m_workCounts[cpuid] >= m_quantumSize ) {
      // hit quantum boundary
      m_stalledAtQuantumBoundary.at( cpuid ) = true;
      InsnCountInducedRoundBoundaries++;
      TotalQuanta++;
      commitThisRound = true;

      // check to see if I'm the last one in
      if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
    }
  } // end basicBlock()

  bool weAreDoneWithQuantumRound() {
    unsigned numNonProgressingCores = 0;
    for ( unsigned i = 0; i < NUM_CORES; i++ ) {
      if ( m_stalledAtQuantumBoundary.at(i) || m_blocked.at(i) || m_waitingForCausality.at(i) ) {
        numNonProgressingCores++;
      }
    }

    return (numNonProgressingCores >= min( NUM_CORES, m_liveThreads ));
  }

  static void clean(Line* l) {
    l->setClean();
  }

  void finishQuantumRound() {
    uint64_t roundRuntime = 0;
    uint64_t firstToFinish = numeric_limits<uint64_t>::max();
    for ( unsigned i = 0; i < NUM_CORES; i++ ) {
      cache_t* cache = m_allCaches.at(i);
      uint64_t coreRuntime = m_insnCounts.at( i ) + cache->timeInMemoryHierarchy;
      roundRuntime = max( roundRuntime, coreRuntime );
      firstToFinish = min( coreRuntime, firstToFinish );
      m_sumOfInsnsPerQuantum += m_insnCounts.at( i );
      m_sumOfCyclesPerQuantum += coreRuntime;

      m_stalledAtQuantumBoundary.at( i ) = false;
      m_insnCounts.at( i ) = 0;
      m_workCounts.at( i ) = 0;
      cache->timeInMemoryHierarchy = 0;
      cache->storeBufferOverflowed = false;

      // clear out store buffers
      if ( !cache->StoreBufferIsEmpty ) {
        // clear L1
        cache->L1cache->visitAllLines( MultiCacheSimulator::clean );
        // clear L2, if present
        if ( cache->L2cache ) {
          cache->L2cache->visitAllLines( MultiCacheSimulator::clean );
        }

        cache->StoreBufferIsEmpty = true;
      }
    }
    Runtime += roundRuntime;
    TotalQuantumImbalance += (roundRuntime - firstToFinish);
    QuantumRounds++;
    if ( commitThisRound ) {
      QuantumRoundCommits++;
      commitThisRound = false;
    }

    //if ( (m_QuantumRounds.get() % 10000) == 0 ) {
    //cerr << "[debug] tso/hb: " << m_simulateTSO << m_simulateHB << " committed " << m_QuantumRounds.get() << " quantum rounds" << endl;
    //}
  }

  void waitForCausality(int tid) {
    m_waitingForCausality.at( cpuOfTid(tid) ) = true;
    if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
  }
  void satisfiedCausality(int tid) {
    m_waitingForCausality.at( cpuOfTid(tid) ) = false;
  }
  bool isWaitingForCausality(int tid) {
    return m_waitingForCausality.at( cpuOfTid(tid) );
  }

  bool stalledAtQuantumBoundary(int tid) {
    return m_stalledAtQuantumBoundary.at( cpuOfTid(tid) );
  }

  void block(int tid) {
    // NB: can't assert that un/block events are properly nested, because of thread:core multiplexing
    //assert( !m_blocked.at( cpuOfTid(tid) ) );
    m_blocked.at( cpuOfTid(tid) ) = true;
    if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
  }
  void unblock(int tid) {
    // NB: can't assert that un/block events are properly nested, because of thread:core multiplexing
    //assert( m_blocked.at( cpuOfTid(tid) ) );
    m_blocked.at( cpuOfTid(tid) ) = false;
  }
  bool isblocked(int tid) {
    return m_blocked.at( cpuOfTid(tid) );
  }

  void setLiveThreads(unsigned n) {
    m_liveThreads = n;
    if ( weAreDoneWithQuantumRound() ) finishQuantumRound();
  }

};
#endif 
