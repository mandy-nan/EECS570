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

#include <iterator>
#include <limits>
#include <ctime>
#include <sys/stat.h>

#include <boost/program_options.hpp>
namespace knob = boost::program_options;

#include "rcdcsim.hpp"

#include "Event.hpp"
#include "Knobs.hpp"
#include "MultiCacheSimulator.hpp"

#include "Counter.hpp"
vector<Counter*> Counter::s_AllStats;

/** program knobs */
static knob::variables_map s_knobs;

/** Map from a sync object => source's vector clock */
map<uint64_t, vector<uint64_t> > g_vcOfSyncObject;

/** Insns executed, cumulative across all threads. */
static uint64_t s_insnsExecuted = 0;
static uint64_t s_stackAccesses = 0;


static string pyOfBool( bool b ) {
  return b ? "True" : "False";
}

static string nameOfDetStrategy() {
  if ( s_knobs.count(KnobTSO) ) return "tso";
  else if ( s_knobs.count(KnobHB) ) return "hb";
  else if ( s_knobs.count(KnobNondet) ) return "nondet";
  else {
    // didn't specify any strategy!
    assert(false);
    return "";
  }
}

/** c/o http://www.techbytes.ca/techbyte103.html */
static bool fileExists( string strFilename ) {
  struct stat stFileInfo;
  bool blnReturn;
  int intStat;

  // Attempt to get the file attributes
  intStat = stat( strFilename.c_str(), &stFileInfo );
  if ( 0 == intStat ) {
    // We were able to get the file attributes
    // so the file obviously exists.
    blnReturn = true;
  } else {
    // We were not able to get the file attributes.
    // This may mean that we don't have permission to
    // access the folder which contains this file. If you
    // need to do that level of checking, lookup the
    // return values of stat which will give you
    // more details on why stat failed.
    blnReturn = false;
  }

  return ( blnReturn );
}


/** Whether to use the store buffer for this memory event or not. */
static bool usesStoreBuffer( const Event& e ) {
  // stack accesses can skip the store buffer
  if ( s_knobs.count(KnobIgnoreStackRefs) ) {
    if ( e.m_stackRef ) {
      s_stackAccesses++;
      return false;
    }
  }

  // uses store buffer
  return true;
}

static uint64_t s_causalityDelays = 0;
static uint64_t s_unprocessedEvents = 0;
static uint64_t s_forcedCommits = 0;

bool syncEventCanProceed(const Event& e,
                         map<uint64_t,uint64_t>& activeEventOfSyncObject,
                         MultiCacheSimulator<RCDCLine, uint64_t>* sim,
                         vector< deque<Event> >& eventBuffers) {
  if ( !e.m_isLifeLock ) {
    return true;
  }

  if ( 1 == e.m_logicalTime ) { // first sync event: ok to execute
    activeEventOfSyncObject[ e.m_syncObject ] = 2;
  } else {
    map<uint64_t,uint64_t>::iterator mit = activeEventOfSyncObject.find( e.m_syncObject );

    if ( activeEventOfSyncObject.end() != mit && mit->second == e.m_logicalTime ) { // I'm the next event!
      activeEventOfSyncObject[ e.m_syncObject ] = e.m_logicalTime + 1;
    } else {
      if ( activeEventOfSyncObject.end() != mit ) {
        assert( e.m_logicalTime > mit->second );
      }
      // I have to wait
      s_causalityDelays++;
      sim->waitForCausality( e.m_tid );
      eventBuffers.at( sim->cpuOfTid( e.m_tid ) ).push_front( e );
      return false;
    }

  }
  sim->satisfiedCausality( e.m_tid );
  return true;
}

static int s_maxLiveThreads = 0;
static int s_numSpawnedThreads = 0;

void processEvents(MultiCacheSimulator<RCDCLine, uint64_t>* sim, ifstream& eventFifo) {

  int currentLiveThreads = 0;

  /** the number of sync events seen thus far (in the trace; these events have
  not necessarily executed yet) for each sync object. Used to enforce a total
  order on sink/source events to a given sync object, based on their order of
  appearance in the event fifo. */
  map<uint64_t, uint64_t> receivedEventsOfSyncObject;

  /** the last source event that was executed for each sync object */
  map<uint64_t, uint64_t> activeEventOfSyncObject;

  vector< deque<Event> > eventBuffers( sim->m_allCaches.size() );

  bool allDone = false;
  bool fifoOpen = true;
  /** the first event buffer to check: we rotate through them to avoid livelock */
  uint64_t firstEventBuffer = 0;
  unsigned iterationsWithoutProgress = 0;
  Event e;
  while ( true ) {

    if ( iterationsWithoutProgress > 100000 ) {
      s_forcedCommits++;
      iterationsWithoutProgress = 0;
      sim->finishQuantumRound();
    }

    // help avoid forced commits by ignoring threads that are finished
    if ( !fifoOpen ) {
      s_unprocessedEvents = 0;
      for ( unsigned i = 0; i < eventBuffers.size(); i++ ) {
        if ( eventBuffers.at(i).empty() ) {
          sim->block( i );
        } else {
          s_unprocessedEvents += eventBuffers.at(i).size();
        }
      }
    }

    // prefer draining per-thread queues over reading from the front-end
    bool tookEventFromLocalBuffer = false;

    /* NB: this code means that we will process a bunch of events for each
    thread, instead of (more realistically) interleaving events from different
    threads. */
    for ( unsigned i = 0; i < eventBuffers.size(); i++ ) {
      const unsigned bufferIndex = (i + firstEventBuffer) % eventBuffers.size();
      if ( !sim->stalledAtQuantumBoundary(bufferIndex) && !eventBuffers.at(bufferIndex).empty() ) {
        e = eventBuffers.at(bufferIndex).front();
        eventBuffers.at(bufferIndex).pop_front();
        tookEventFromLocalBuffer = true;
        break;
      }
    }
    firstEventBuffer++;

    if ( fifoOpen && !tookEventFromLocalBuffer ) {
      // blocking read from fifo
      assert( eventFifo.good() );
      eventFifo.read( (char*) &e, sizeof(Event) );

      const unsigned bytesRead = eventFifo.gcount();

      if ( 0 == bytesRead ) {
        assert( eventFifo.eof() );
        fifoOpen = false;
        iterationsWithoutProgress++;
        continue;
      }

      assert( sizeof(Event) == bytesRead );
      int cpuid = sim->cpuOfTid( e.m_tid );

      // enforce a total order on sync events for a given sync object
      if ( e.m_isLifeLock ) {
        switch ( e.m_type ) {
        case HAPPENS_BEFORE_SOURCE:
        case HAPPENS_BEFORE_SINK: {
          map<uint64_t,uint64_t>::iterator mit = receivedEventsOfSyncObject.find( e.m_syncObject );
          if ( mit == receivedEventsOfSyncObject.end() ) {
            // NB: logical times start at 1
            receivedEventsOfSyncObject[ e.m_syncObject ] = 1;
            e.m_logicalTime = 1;
          } else {
            uint64_t newval = mit->second + 1;
            receivedEventsOfSyncObject[ e.m_syncObject ] = newval;
            e.m_logicalTime = newval;
          }
        }
          break;
        default:
          break;
        }
      }

      // core is stalled, so buffer the event locally
      if ( sim->stalledAtQuantumBoundary(e.m_tid) ) {
        eventBuffers.at( cpuid ).push_back( e );
        iterationsWithoutProgress++;
        continue;
      } else {
        // if core is not stalled, we should have drained its buffer already
        assert( eventBuffers.at(cpuid).empty() );
      }

    }

    if( sim->stalledAtQuantumBoundary(e.m_tid) ) {
      iterationsWithoutProgress++;
      continue;
    }

    bool madeProgress = true;

    // event dispatch
    switch ( e.m_type ) {

    case ROI_START:
    case ROI_FINISH:
      // NOPs for now
      break;

    case THREAD_START:
      currentLiveThreads++;
      sim->setLiveThreads( currentLiveThreads );
      s_numSpawnedThreads++;
      s_maxLiveThreads = max( s_maxLiveThreads, currentLiveThreads );
      break;

    case THREAD_FINISH:
      currentLiveThreads--;
      sim->setLiveThreads( currentLiveThreads );
      sim->block( e.m_tid );
      // when main thread exits, tear down simulation
      if ( 0 == e.m_tid ) {
        allDone = true;
      }
      break;

    case THREAD_BLOCKED:
      sim->block( e.m_tid );
      break;
    case THREAD_UNBLOCKED:
      sim->unblock( e.m_tid );
      break;

    case MEMORY_ALLOCATION:
      // NOP for now
      break;
    case MEMORY_FREE:
      // NOP for now
      break;

    case HAPPENS_BEFORE_SOURCE: {
      if ( syncEventCanProceed(e, activeEventOfSyncObject, sim, eventBuffers) ) {
        sim->syncOp( e.m_tid, SYNC_SOURCE, false, INVALID_THREADID, e.m_syncObject );
      } else madeProgress = false;
    }
      break;
    case HAPPENS_BEFORE_SINK: {
      if ( syncEventCanProceed(e, activeEventOfSyncObject, sim, eventBuffers) ) {
        sim->syncOp( e.m_tid, SYNC_SINK, e.m_hbSourceThread != INVALID_THREADID,
                     e.m_hbSourceThread, e.m_syncObject );
      } else madeProgress = false;
    }
      break;

    case MEMORY_READ:
      sim->cacheRead( e.m_tid, e.m_addr, e.m_memOpSize, usesStoreBuffer( e ) );
      break;
    case MEMORY_WRITE:
      sim->cacheWrite( e.m_tid, e.m_addr, e.m_memOpSize, usesStoreBuffer( e ) );
      break;

    case BASIC_BLOCK:
      s_insnsExecuted += e.m_insnCount;
      //if ( (s_insnsExecuted % 5000000) < e.m_insnCount ) {
      //cerr << "[debug] (nd/tso/hb):" << s_knobs.count(KnobNondet) << s_knobs.count(KnobTSO) << s_knobs.count(KnobHB) << " executed " << s_insnsExecuted << " insns" << endl;
      //}
      sim->basicBlock( e.m_tid, e.m_insnCount );
      break;

    case INVALID_EVENT:
    default:
      cerr << e.toString() << endl;
      assert(false);
    }

    iterationsWithoutProgress = madeProgress ? 0 : iterationsWithoutProgress+1;

    if ( allDone ) {

      // there shouldn't be any queued-up events
      s_unprocessedEvents = 0;
      for ( unsigned i = 0; i < eventBuffers.size(); i++ ) {
        s_unprocessedEvents += eventBuffers.at(i).size();
      }

      return;
    }

  } // end while(true)

} // end processEvents()


int main(int argc, char** argv) {

  // Declare the supported options.
  knob::options_description desc("Allowed options");
  desc.add_options()
          ("help", "produce help message")

          (KnobStatsFile, knob::value<string>()->default_value("rcdcsim-stats.py"), "stats file to generate")
          (KnobToSimulatorFifo, knob::value<string>(), "named fifo used to get events from the front-end")

          // these are used to tag the output file, but don't affect the simulation at all
          (KnobScheme, knob::value<string>()->default_value("<none/>"), "text describing this simulation setup")
          (KnobWorkload, knob::value<string>()->default_value("<none/>"), "workload being run")
          (KnobInput, knob::value<string>()->default_value("<none/>"), "input being used")
          (KnobThreads, knob::value<unsigned>()->default_value(0), "number of threads in the workload")

          // everything below are simulator parameters

          (KnobCores, knob::value<unsigned>()->default_value(8), "number of cores to simulate")

          (KnobIgnoreStackRefs, "Ignore stack accesses." )

          // cache parameters
          (KnobBlockSize, knob::value<unsigned>()->default_value(64), "Block size for all caches")
          (KnobL1Size, knob::value<unsigned>()->default_value(1<<15/*32KB*/), "Size (in bytes) of each private L1 cache")
          (KnobL1Assoc, knob::value<unsigned>()->default_value(8), "Associativity of each private L1 cache")

          (KnobUseL2, "Model a private L2 for each core")
          (KnobL2Size, knob::value<unsigned>()->default_value(1<<18/*256KB*/), "Size (in bytes) of the private L2 cache")
          (KnobL2Assoc, knob::value<unsigned>()->default_value(8), "Associativity of the private L2 cache")

          (KnobUseL3, "Model an L3 cache shared amongst all cores")
          (KnobL3Size, knob::value<unsigned>()->default_value(1<<23/*8MB*/), "Size (in bytes) of the shared L3 cache")
          (KnobL3Assoc, knob::value<unsigned>()->default_value(16), "Associativity of the shared L3 cache")

          // RCDC
          (KnobTSO, "Enable simulation of Det-TSO.  Mutually exclusive with other Det-X schemes." )
          (KnobHB, "Enable simulation of Det-HB.  Mutually exclusive with other Det-X schemes." )
          (KnobNondet, "Enable simulation of Nondet.  Mutually exclusive with other Det-X schemes." )
          (KnobQuantumSize, knob::value<unsigned>()->default_value(1000), "Quantum size (insns)")
          (KnobSmartQuantumBuilding, "Use store buffer hit/miss information to deterministically estimate runtime when possible." )
          ;

  knob::store( knob::parse_command_line(argc, argv, desc), s_knobs );
  knob::notify(s_knobs);

  if (s_knobs.count("help")) {
      cout << desc << "\n";
      return 1;
  }

  // can only simulate one execution strategy at a time
  assert( s_knobs.count(KnobTSO) + s_knobs.count(KnobHB) + s_knobs.count(KnobNondet) == 1 );

  time_t startTime = time( NULL );

  MultiCacheSimulator<RCDCLine, uint64_t>* sim;
  CacheConfiguration<RCDCLine> l1config, l2config, l3config;
  l1config.blockSize = l2config.blockSize = l3config.blockSize = s_knobs[KnobBlockSize].as<unsigned>();
  l1config.callbacks = l2config.callbacks = l3config.callbacks = NULL;

  l1config.assoc = s_knobs[KnobL1Assoc].as<unsigned>();
  l1config.cacheSize = s_knobs[KnobL1Size].as<unsigned>();

  l2config.assoc = s_knobs[KnobL2Assoc].as<unsigned>();
  l2config.cacheSize = s_knobs[KnobL2Size].as<unsigned>();

  l3config.assoc = s_knobs[KnobL3Assoc].as<unsigned>();
  l3config.cacheSize = s_knobs[KnobL3Size].as<unsigned>();

  sim = new MultiCacheSimulator<RCDCLine, uint64_t> (
      s_knobs[KnobCores].as<unsigned>(),
      l1config,
      s_knobs.count(KnobUseL2), l2config,
      s_knobs.count(KnobUseL3), l3config );

  // pass knobs values through to the caches
  SMPCache<RCDCLine, uint64_t>::cache_iter_t it;
  sim->m_simulateHB = s_knobs.count(KnobHB);
  sim->m_simulateTSO = s_knobs.count(KnobTSO);
  sim->m_quantumSize = s_knobs[KnobQuantumSize].as<unsigned>();
  sim->m_smartQuantumBuilding = s_knobs.count(KnobSmartQuantumBuilding);
  for ( it = sim->m_allCaches.begin(); it != sim->m_allCaches.end(); it++ ) {
    // per-cache initialization goes here
    (*it)->useDetStoreBuffers = (sim->m_simulateHB || sim->m_simulateTSO);
  }

  ifstream eventFifo;
  eventFifo.open( s_knobs[KnobToSimulatorFifo].as<string>().c_str(), ios::binary );
  assert( eventFifo.good() );


  // main event loop
  processEvents( sim, eventFifo );



  cerr << "[rcdcsim] generating stats for " << nameOfDetStrategy() << endl;

  eventFifo.close();

  // each stat is dumped as a Python dictionary object
  stringstream prefix;
  prefix << "{'RCDCStat':True, ";

  prefix << "'determinismStrategy': '" << nameOfDetStrategy() << "', ";
  prefix << "'quantumSize': '" << (s_knobs[KnobQuantumSize].as<unsigned>()/1000) << "k', ";
  prefix << "'smartQuantumBuilding': " << pyOfBool( s_knobs.count(KnobSmartQuantumBuilding) ) << ", ";

  prefix << "'threads': " << s_knobs[KnobThreads].as<unsigned>() << ", ";
  prefix << "'workload': '" << s_knobs[KnobWorkload].as<string>() << "', ";
  prefix << "'input': '" << s_knobs[KnobInput].as<string>() << "', ";
  prefix << "'scheme': '" << s_knobs[KnobScheme].as<string>() << "', ";

  prefix << "'cores': '" << s_knobs[KnobCores].as<unsigned>() << "p', ";

  prefix << "'ignoreStackRefs': " << pyOfBool( s_knobs.count(KnobIgnoreStackRefs) ) << ", ";

  prefix << "'BlockSize': " << s_knobs[KnobBlockSize].as<unsigned>() << ", ";
  prefix << "'l1Assoc': " << s_knobs[KnobL1Assoc].as<unsigned>() << ", ";
  prefix << "'l1Size': " << s_knobs[KnobL1Size].as<unsigned>() << ", ";

  prefix << "'useL2': " << pyOfBool( s_knobs.count(KnobUseL2) ) << ", ";
  prefix << "'l2Assoc': " << s_knobs[KnobL2Assoc].as<unsigned>() << ", ";
  prefix << "'l2Size': " << s_knobs[KnobL2Size].as<unsigned>() << ", ";

  prefix << "'useL3': " << pyOfBool( s_knobs.count(KnobUseL3) ) << ", ";
  prefix << "'l3Assoc': " << s_knobs[KnobL3Assoc].as<unsigned>() << ", ";
  prefix << "'l3Size': " << s_knobs[KnobL3Size].as<unsigned>() << ", ";

  // actual stat value goes here
  string suffix = "}\n";

  // check for filename collisions and rename around them
  string statsFilename = s_knobs["statsfile"].as<string>();
  while ( fileExists( statsFilename ) ) {
    statsFilename += ".1";
  }
  ofstream statsFile( statsFilename.c_str(), ios_base::trunc );

  // dump stats from the caches
  sim->dumpStats( statsFile, prefix.str(), suffix );

  // dump "global" stats

  double minutes = difftime( time( NULL ), startTime ) / 60.0;
  statsFile << prefix.str() << "'SimulationRunningTimeMinutes': " << minutes << suffix;

  statsFile << prefix.str() << "'maxLiveThreads': " << s_maxLiveThreads << suffix;
  statsFile << prefix.str() << "'numSpawnedThreads': " << s_numSpawnedThreads << suffix;
  statsFile << prefix.str() << "'numStackAccesses': " << s_stackAccesses << suffix;
  statsFile << prefix.str() << "'numTotalInstructions': " << s_insnsExecuted << suffix;
  statsFile << prefix.str() << "'causalityInducedEventDelays': " << s_causalityDelays << suffix;
  statsFile << prefix.str() << "'unprocessedEvents': " << s_unprocessedEvents << suffix;
  statsFile << prefix.str() << "'forcedCommits': " << s_forcedCommits << suffix;

  statsFile.close();
  cerr << "[rcdcsim] finished generating stats for " << nameOfDetStrategy() << endl;

  delete sim;

  cerr << "[rcdcsim] simulation process exiting" << endl;

  return 0;
}
