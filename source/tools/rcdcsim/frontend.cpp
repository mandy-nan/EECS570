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

/**
 * Sets up the simulation, by registering callbacks with Pin and launching the simulation thread.
 */

#include "frontend.hpp"
#include "PinCallbacks.hpp"

KNOB<string> KnobToSimulatorFifo( KNOB_MODE_WRITEONCE, "pintool", "tosim-fifo",
                                  "tosim", "The named fifo used to send events to the simulator." );
KNOB<unsigned> KnobCores( KNOB_MODE_WRITEONCE, "pintool", "cores",
                          "1", "Number of simulated cores." );

/** for interacting with the message threads */
static PIN_THREAD_UID s_IOThreadId;

VOID instrumentTrace( TRACE trace, VOID *v ) {
  for ( BBL bbl = TRACE_BblHead( trace ); BBL_Valid( bbl ); bbl = BBL_Next( bbl ) ) {
    INS ins = BBL_InsHead( bbl );

    INS_InsertCall( ins, IPOINT_BEFORE, (AFUNPTR) startBasicBlock, IARG_THREAD_ID,
                    IARG_CONTEXT, IARG_UINT32, BBL_NumIns( bbl ), IARG_END );

    UINT32 instPos = 0;
    for ( ; INS_Valid( ins ); ins = INS_Next( ins ) ) {
      if ( INS_IsMemoryRead( ins ) ) {
        INS_InsertCall( ins, IPOINT_BEFORE, (AFUNPTR) memOp, IARG_THREAD_ID, IARG_UINT32,
                        instPos, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_BOOL,
                        true, IARG_BOOL, INS_IsStackRead( ins ), IARG_END );
      }

      if ( INS_IsMemoryWrite( ins ) ) {
        INS_InsertCall( ins, IPOINT_BEFORE, (AFUNPTR) memOp, IARG_THREAD_ID, IARG_UINT32,
                        instPos, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_BOOL,
                        false, IARG_BOOL, INS_IsStackWrite( ins ), IARG_END );
      }
      instPos++;
    }
  }
}

BOOL isLikeLockAcquire( const char *rtnName ) {
  return strstr( rtnName, "pthread_mutex_lock" ) || //
      strstr( rtnName, "pthread_cond_wait" ) || //
      strstr( rtnName, "pthread_cond_timedwait" ) || //
      strstr( rtnName, "ACQUIRE_FENCE" ); //custom hook for canneal
}

BOOL isLikeLockRelease( const char *rtnName ) {
  return strstr( rtnName, "pthread_mutex_unlock" ) || //
      strstr( rtnName, "pthread_cond_broadcast" ) || //
      strstr( rtnName, "pthread_cond_signal" ) || //
      strstr( rtnName, "RELEASE_FENCE" ); // custom hook for canneal
}

VOID instrumentImage( IMG img, VOID *v ) {

  for ( SEC sec = IMG_SecHead( img ); SEC_Valid( sec ); sec = SEC_Next( sec ) ) {
    for ( RTN rtn = SEC_RtnHead( sec ); RTN_Valid( rtn ); rtn = RTN_Next( rtn ) ) {
      const char *rtnName = RTN_Name( rtn ).c_str();

      if ( strstr( rtnName, "__parsec_roi_begin" ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) setStartReached, IARG_THREAD_ID,
                        IARG_END );
        RTN_Close( rtn );

      } else if ( strstr( rtnName, "__parsec_roi_end" ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) setEndReached, IARG_THREAD_ID,
                        IARG_END );
        RTN_Close( rtn );

      } else if ( RTN_Name( rtn ) == "malloc" ) { // in g++, new does not call malloc()
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeMalloc, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // size requested
                        IARG_END );
        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterMalloc, IARG_THREAD_ID,
                        IARG_FUNCRET_EXITPOINT_VALUE, // pointer to allocation
                        IARG_END );
        RTN_Close( rtn );
      } else if ( RTN_Name( rtn ) == "_Znwm" ) { // g++'s new
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeMalloc, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // size requested
                        IARG_END );
        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterMalloc, IARG_THREAD_ID,
                        IARG_FUNCRET_EXITPOINT_VALUE, // pointer to allocation
                        IARG_END );
        RTN_Close( rtn );

      } else if ( RTN_Name( rtn ) == "free" ) { // in g++, delete does not call free()
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeFree, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // pointer to free
                        IARG_END );
        RTN_Close( rtn );
      } else if ( RTN_Name( rtn ) == "_ZdlPv" ) { // g++'s delete
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeFree, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // pointer to free
                        IARG_END );
        RTN_Close( rtn );

      } else if ( strstr( rtnName, "pthread_self" ) ) {
        assert( RTN_Valid(rtn) );
        realPthreadSelf = (AFUNPTR) RTN_Address( rtn );

      } else if ( strstr( rtnName, "pthread_create" ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforePthreadCreate,
                        IARG_THREAD_ID, //
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // the pthread_t*
                        IARG_END );
        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterPthreadCreate, IARG_THREAD_ID,
                        IARG_END );
        RTN_Close( rtn );

      } else if ( strstr( rtnName, "pthread_join" ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeJoin, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END );
        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterJoin, IARG_THREAD_ID, IARG_END );
        RTN_Close( rtn );

      } else if ( isLikeLockAcquire( rtnName ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeLockAcquire, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END );

        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterLockAcquire, IARG_THREAD_ID,
                        IARG_END );
        RTN_Close( rtn );
      } else if ( strstr( rtnName, "pthread_mutex_trylock" ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeTrylock, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END );
        RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR) afterTrylock, IARG_THREAD_ID,
                        IARG_FUNCRET_EXITPOINT_VALUE, IARG_END );
        RTN_Close( rtn );

      } else if ( isLikeLockRelease( rtnName ) ) {
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) beforeLockRelease, IARG_THREAD_ID,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END );
        RTN_Close( rtn );

      } else {
        // see threadBegin() for why we need to instrument all function calls
        RTN_Open( rtn );
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR) startFunctionCall, IARG_THREAD_ID,
                        IARG_CONTEXT, IARG_END );
        RTN_Close( rtn );
      }

      // TODO: pthread_mutex_timedlock, reader-writer locks

    } // for RTN
  } // for SEC
} // end instrumentImage()


// from PinCallbacks.cpp
void initCallbackStuff();
void ioThread(void* arg);

void initRcdcSim() {
  initCallbackStuff();
}

VOID pinFini( INT32 code, VOID *v ) {
  VERBOSE(stderr, "pinFini: %d\n", code);

  // wait for the IO thread to terminate
  /*bool done =*/ PIN_WaitForThreadTermination( s_IOThreadId, PIN_INFINITE_TIMEOUT, NULL );
  //assert( done ); // get a core dump if the IO thread times out

  cerr << "[rcdcsim] front end pintool finished" << endl;
}

/* ===================================================================== */

INT32 Usage() {
  cerr << "This PIN tool is an RCDC simulator.\n\n";
  cerr << KNOB_BASE::StringKnobSummary();
  cerr << endl;
  return -1;
}

int main( int argc, char *argv[] ) {

  if ( PIN_Init( argc, argv ) ) {
    return Usage();
  }
  PIN_InitSymbols();

  initRcdcSim();

  IMG_AddInstrumentFunction( instrumentImage, NULL );
  TRACE_AddInstrumentFunction( instrumentTrace, NULL );

  PIN_AddThreadStartFunction( threadBegin, NULL );
  PIN_AddThreadFiniFunction( threadEnd, NULL );

  PIN_AddContextChangeFunction( beforeSignal, NULL );
  PIN_AddSyscallEntryFunction( beforeSyscall, NULL );
  PIN_AddSyscallExitFunction( afterSyscall, NULL );

  PIN_AddFiniUnlockedFunction( pinFini, NULL );

  if ( !CODECACHE_ChangeMaxInsPerTrace( 4096 * 1024 ) ) {
    fprintf( stderr, "TLSProf::CODECACHE_ChangeMaxInsPerTrace failed.\n" );
  }

  THREADID tid = PIN_SpawnInternalThread( ioThread, NULL, 0, &s_IOThreadId );
  assert( tid != INVALID_THREADID );

  PIN_StartProgram();
  return 0;
}
