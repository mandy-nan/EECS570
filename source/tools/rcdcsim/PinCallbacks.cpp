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
 * All Pin callbacks live here.  They put events into queues that are
 * then consumed by the simulation thread.
 */

#include <unistd.h>
#include <iostream>

#include "frontend.hpp"
#include "PinCallbacks.hpp"

#define SIMULATOR_FRONTEND
#include "Event.hpp"
#undef SIMULATOR_FRONTEND

#include <boost/atomic.hpp>
#include <boost/lockfree/ringbuffer.hpp>

/** Queue of events from the program threads to the I/O thread. */
static boost::lockfree::ringbuffer<Event, 0> s_EventQueue( 100000/*max event queue size*/);

/** path to the fifos */
extern KNOB<string> KnobToSimulatorFifo;
extern KNOB<unsigned> KnobCores;


/* What are "life locks"?

 Life locks (LLs) are synthetic sync objects used to encode the HB orderings of
 thread create and join.  For simplicity, a LL is just the pthread_t of the child
 thread.

 Currently, we enforce an HB edge from just after the pthread_create() call to
 before the first function that runs in the child thread.  After pthread_create()
 returns, the parent thread "releases" the life lock and sets a flag.  The child
 thread, at its first function call, 1) determines its pthread_t, 2) updates the
 tid=>pthread_t mapping and 3) waits for the parent thread to set the flag. After
 the flag has been set, the child thread continues and "acquires" the life lock.
 This flag-waiting ensures that the acquire happens-after the release.
 */

/** Lookup a pthread_t from a THREADID. Used by Pin endThread() hook. */
static map<THREADID, pthread_t> s_PthreadOfTid;
static set<pthread_t> s_PthreadsRegistered;
/** Lock for s_PthreadOfTid and s_PthreadsRegistered */
static PinLock sl_LifeLock;

/** function pointer to pthread_self() */
AFUNPTR realPthreadSelf = NULL;

/** Thread-local storage to pass sync object info between before...() and after...() calls. */
static TLS_KEY t_SyncObjectUsed;
/** Indicates the kind of sync operation whose data is stored in
 * t_SyncObjectUsed.  Helps with debugging nested use of TLS. */
static TLS_KEY t_PreviousSyncOperation;
static const char* CREATE_OP = "create";
static const char* JOIN_OP = "join";
static const char* LOCK_OP = "lock";
static const char* TRYLOCK_OP = "trylock";

/** Indicates whether this thread hasn't yet registered its id information.
 * Logical inversion used because the default key value is NULL. */
static TLS_KEY t_ThreadInfoUnregistered;

/** The size of a malloc request, so we know both size and location when malloc() returns. */
static TLS_KEY t_MallocSize;

map<ADDRINT, THREADID> Event::s_WhoLastAccessed;
PinLock Event::sl_LastAccessed;

/** Initializes thread-local storage used by Pin callbacks. Needs to be run once
 from the main thread before any instrumentation occurs. */
void initCallbackStuff() {
  t_SyncObjectUsed = PIN_CreateThreadDataKey( NULL );
  t_PreviousSyncOperation = PIN_CreateThreadDataKey( NULL );
  t_ThreadInfoUnregistered = PIN_CreateThreadDataKey( NULL );
  t_MallocSize = PIN_CreateThreadDataKey( NULL );
}

// macro-ified to get "backtrace"
//#define setTlsSyncObj(value,tid,type) {\
  VOID* data = PIN_GetThreadData( t_SyncObjectUsed, tid );\
  if ( data != NULL ) {\
    cout << "previous op: " << (char*)PIN_GetThreadData( t_PreviousSyncOperation, tid ) << endl;\
  }\
  assert( data == NULL );\
  BOOL ok = PIN_SetThreadData( t_SyncObjectUsed, value, tid );\
  assert( ok );\
  ok = PIN_SetThreadData( t_PreviousSyncOperation, type, tid );\
  assert( ok );\
}

static void setTlsSyncObj( VOID* value, THREADID tid, const char* type ) {
  //  VOID* data = PIN_GetThreadData( t_SyncObjectUsed, tid );
  //  if ( data != NULL ) {
  //    cout << "previous op: " << (char*)PIN_GetThreadData( t_PreviousSyncOperation, tid ) << endl;
  //  }
  //  assert( data == NULL );
  BOOL ok = PIN_SetThreadData( t_SyncObjectUsed, value, tid );
  assert( ok );
  //  ok = PIN_SetThreadData( t_PreviousSyncOperation, type, tid );
  //  assert( ok );
}

static VOID* getTlsSyncObj( THREADID tid ) {
  VOID* data = PIN_GetThreadData( t_SyncObjectUsed, tid );
  assert( data != NULL );

  // overwrite data so we can detect nesting
  //  BOOL ok = PIN_SetThreadData( t_SyncObjectUsed, NULL, tid );
  //  assert( ok );
  //  ok = PIN_SetThreadData( t_PreviousSyncOperation, NULL, tid );
  //  assert( ok );

  return data;
}

/** Writes events into the fifo to send them to the simulator. */
void ioThread(void* unused) {

  // we want the simulator process(es) to run with higher priority than the frontend
  int r = nice( 5 );
  assert( -1 != r );
  cerr << "[frontend] new nice value: " << r << endl;

  ofstream eventFifo;
  eventFifo.open( KnobToSimulatorFifo.Value().c_str(), ios::binary );

  Event e;

  while (true) {

    bool success = s_EventQueue.dequeue( &e );
    if ( success ) {
      assert( eventFifo.good() );
      // this is a blocking write
      eventFifo.write( (const char*) &e, sizeof(e) );

      if ( e.m_type == THREAD_FINISH && 0 == e.m_tid ) {
        // when main thread exits, tear down simulation
        break;
      }
    }

  } // end infinite loop

  eventFifo.flush();
  eventFifo.close();

  PIN_ExitThread( 0 );
} // end ioThread()


static void addEvent( Event e ) {
  switch ( e.m_type ) {
  case INVALID_EVENT:
    //VERBOSE(stderr, "%s\n", e.toString().c_str());
    assert(false);
    break;
  case ROI_START:
  case ROI_FINISH:
  case THREAD_START:
  case THREAD_FINISH:
  case THREAD_BLOCKED:
  case THREAD_UNBLOCKED:
  case HAPPENS_BEFORE_SOURCE:
  case HAPPENS_BEFORE_SINK:
    VERBOSE_SYNC( stderr, "%s\n", e.toString().c_str() );
    break;
  case MEMORY_READ:
  case MEMORY_WRITE:
    VERBOSE_MEM(stderr, "%s\n", e.toString().c_str());
    break;
  case MEMORY_ALLOCATION:
  case MEMORY_FREE:
    VERBOSE_ALLOC(stderr, "%s\n", e.toString().c_str());
    break;
  case BASIC_BLOCK:
    VERBOSE(stderr, "%s\n", e.toString().c_str());
    break;
  default:
    assert(false);
  }

  // block until there's a free slot in the queue
  bool success = false;
  do {
    success = s_EventQueue.enqueue( e );
    PIN_Yield();
  } while ( !success );
} // end addEvent()

void setStartReached( THREADID tid ) {
  addEvent( Event::ThreadEvent( tid, ROI_START ) );
}
void setEndReached( THREADID tid ) {
  addEvent( Event::ThreadEvent( tid, ROI_FINISH ) );
}

void beforeMalloc( THREADID tid, ADDRINT size ) {
  ADDRINT sz = (ADDRINT) PIN_GetThreadData( t_MallocSize, tid );
  if ( sz != 0 ) {
    // we're already inside an allocation
    return;
  }

  BOOL ok = PIN_SetThreadData( t_MallocSize, (VOID*) size, tid );
  assert( ok );
}
void afterMalloc( THREADID tid, ADDRINT pointer ) {
  ADDRINT size = (ADDRINT) PIN_GetThreadData( t_MallocSize, tid );
  if ( 0 != pointer && 0 != size ) { // ignore malloc() failures
    addEvent( Event::AllocationEvent( tid, MEMORY_ALLOCATION, pointer, size ) );
  }

  // clear out t_MallocSize so we handle re-entrancy
  BOOL ok = PIN_SetThreadData( t_MallocSize, NULL, tid );
  assert( ok );
}
void beforeFree( THREADID tid, ADDRINT pointer ) {
  if ( 0 != pointer ) {
    addEvent( Event::AllocationEvent(tid, MEMORY_FREE, pointer, 0 ) );
  }
}

/** Runs before every function call.  See threadBegin() for more details. */
void startFunctionCall( THREADID tid, CONTEXT* ctxt ) {
  // Main thread doesn't have a life lock.  Make sure we can call pthread_self().
  if ( 0 == tid || NULL == realPthreadSelf ) {
    return;
  }

  // inject the life lock acquire if we haven't yet
  VOID* data = PIN_GetThreadData( t_ThreadInfoUnregistered, tid );
  BOOL unregisteredIdentifyInfo = (BOOL) data;
  if ( data != NULL && unregisteredIdentifyInfo ) {

    // find my pthread_t by calling pthread_self()
    pthread_t myPthreadT;
    PIN_CallApplicationFunction( ctxt, tid, CALLINGSTD_DEFAULT, realPthreadSelf,
                                 PIN_PARG( pthread_t ), &myPthreadT, PIN_PARG_END() );
    VERBOSE_SYNC( stderr, "startFunctionCall: pthread_self():%p myPthread:%8lx tid:%u\n",
        realPthreadSelf, myPthreadT, tid );

    // register our tid=>pthread_t mapping
    sl_LifeLock.lock();
    s_PthreadOfTid[tid] = myPthreadT;
    sl_LifeLock.unlock();

    // wait for afterPthreadCreate() to register our pthread_t
    while ( true ) {
      sl_LifeLock.lock();
      bool afterPthreadCreateRan = false;
      if ( s_PthreadsRegistered.count( myPthreadT ) > 0 ) {
        afterPthreadCreateRan = true;
      }
      sl_LifeLock.unlock();
      if ( afterPthreadCreateRan ) {
        break;
      }
      PIN_Yield();
    }

    // we know our life lock has been released, so we can "acquire" it now
    addEvent( Event::SyncSinkEvent( tid, HAPPENS_BEFORE_SINK, myPthreadT, true ) );

    BOOL ok = PIN_SetThreadData( t_ThreadInfoUnregistered, (VOID*) false, tid );
    assert( ok );
  }
}

void startBasicBlock( THREADID tid, CONTEXT* ctxt, UINT32 insnCount ) {
  addEvent( Event::BasicBlockEvent( tid, BASIC_BLOCK, insnCount ) );
}
void memOp( THREADID tid, UINT32 pos, ADDRINT addr, UINT32 size, BOOL isRead,
            BOOL isStackRef ) {
		  ADDRINT read;
		  ADDRINT write;
		  if(isRead){
		  PIN_SafeCopy(&read,&addr,sizeof(ADDRINT));
		  std::cout << "load value:	" << read  << "	address:	" << addr << endl;
		  } 
		  else{
		  PIN_SafeCopy(&write,&addr,sizeof(ADDRINT));
		  std::cout << "store value:	" << write << "	address:	" << addr << endl; 
		  }	
		  addEvent( Event::MemoryEvent( tid, isRead ? MEMORY_READ : MEMORY_WRITE, addr, size, isStackRef ) );

}

// Thread creation/deletion stuff

void threadBegin( THREADID tid, CONTEXT* ctxt, INT32 flags, VOID *v ) {
  addEvent( Event::ThreadEvent( tid, THREAD_START ) );

  BOOL ok = PIN_SetThreadData( t_ThreadInfoUnregistered, (VOID*) true, tid );
  assert( ok );

  // this must start out NULL
  ok = PIN_SetThreadData( t_SyncObjectUsed, NULL, tid );
  assert( ok );
}
void threadEnd( THREADID tid, const CONTEXT* ctx, INT32 code, VOID *v ) {

  if ( tid != 0 ) { // main thread has no life lock
    // lookup this thread's life lock
    sl_LifeLock.lock();
    pthread_t me = s_PthreadOfTid.at( tid );
    sl_LifeLock.unlock();

    // "release" the life lock
    addEvent( Event::SyncSourceEvent( tid, HAPPENS_BEFORE_SOURCE, me, true ) );
  }

  addEvent( Event::ThreadEvent( tid, THREAD_FINISH ) );
}

void beforePthreadCreate( THREADID tid, pthread_t* childPthreadT ) {
  // pass the pthread_t* to the afterPthreadCreate() call
  setTlsSyncObj( (VOID*) childPthreadT, tid, CREATE_OP );
}
void afterPthreadCreate( THREADID tid ) {
  VOID* data = getTlsSyncObj( tid );
  pthread_t child = *( (pthread_t*) data );

  sl_LifeLock.lock();
  s_PthreadsRegistered.insert( child );
  sl_LifeLock.unlock();

  // "release" the life lock
  addEvent( Event::SyncSourceEvent( tid, HAPPENS_BEFORE_SOURCE, child, true ) );
}

void beforeJoin( THREADID tid, ADDRINT pthread_t ) {
  VERBOSE_SYNC( stderr, "beforeJoin: t:%u pthread:%8lx\n", tid, pthread_t );
  // record the thread we're trying to join with so we have the information after the join
  assert( pthread_t != 0 );
  setTlsSyncObj( (VOID*) pthread_t, tid, JOIN_OP );
  addEvent( Event::ThreadEvent(tid, THREAD_BLOCKED) );
}
void afterJoin( THREADID tid ) {
  addEvent( Event::ThreadEvent(tid, THREAD_UNBLOCKED) );

  // we just joined on the thread we were trying to join with
  ADDRINT pthread = (ADDRINT) getTlsSyncObj( tid );

  VERBOSE_SYNC( stderr, "afterJoin: t:%u pthread:%8lx\n", tid, pthread );

  // "acquire" the life lock
  addEvent( Event::SyncSinkEvent( tid, HAPPENS_BEFORE_SINK, pthread, true ) );
}

/* NB: we're ignoring nesting of these calls.  ferret did reliably trigger
 nesting of beforeLockAcquire() calls, but many other benchmarks don't so we're
 just going to ignore it for now. */

void beforeLockAcquire( THREADID tid, ADDRINT lockAddr ) {
  VERBOSE_SYNC( stderr, "beforeLockAcquire: t:%u lock:%8lx\n", tid, lockAddr );
  // record the lock we're trying to acquire so we have the information after the acquire
  assert( lockAddr != 0 );
  setTlsSyncObj( (VOID*) lockAddr, tid, LOCK_OP );
  addEvent( Event::ThreadEvent(tid, THREAD_BLOCKED) );
}

void afterLockAcquire( THREADID tid ) {
  addEvent( Event::ThreadEvent(tid, THREAD_UNBLOCKED) );

  // we just acquired the lock we were trying to acquire
  ADDRINT lockAddr = (ADDRINT) getTlsSyncObj( tid );
  VERBOSE_SYNC( stderr, "afterLockAcquire: t:%u lock:%8lx\n", tid, lockAddr );

  addEvent( Event::SyncSinkEvent( tid, HAPPENS_BEFORE_SINK, lockAddr, false ) );
}

void beforeLockRelease( THREADID tid, ADDRINT lockAddr ) {
  VERBOSE_SYNC( stderr, "lockRelease: t:%u lock:%8lx\n", tid, lockAddr );

  addEvent( Event::SyncSourceEvent( tid, HAPPENS_BEFORE_SOURCE, lockAddr, false ) );
}

void beforeTrylock( THREADID tid, ADDRINT lockAddr ) {
  VERBOSE_SYNC( stderr, "beforeTrylock: t:%u lock:%8lx\n", tid, lockAddr );

  assert( lockAddr != 0 );
  setTlsSyncObj( (VOID*) lockAddr, tid, TRYLOCK_OP );

  // Don't issue any event yet: we generate the event based on the return value of trylock()
}

void afterTrylock( THREADID tid, ADDRINT outcome ) {
  ADDRINT lockAddr = (ADDRINT) getTlsSyncObj( tid );

  VERBOSE_SYNC( stderr, "afterTrylock: t:%u lock:%p result:%lu\n", tid, (void*) lockAddr,
      outcome );

  if ( 0 != outcome ) {
    // we didn't get the lock
    return;
  }

  addEvent( Event::SyncSinkEvent( tid, HAPPENS_BEFORE_SINK, lockAddr, false ) );
}

// Currently unused systemcall hooks.  Used to detect when threads are blocked, which isn't
// really necessary with the single-queue model.

void beforeSignal( THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *from,
                   CONTEXT *to, INT32 info, VOID *v ) {
}

void beforeSyscall( THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD sys, VOID* unused ) {
}

void afterSyscall( THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD sys, VOID* unused ) {
}
