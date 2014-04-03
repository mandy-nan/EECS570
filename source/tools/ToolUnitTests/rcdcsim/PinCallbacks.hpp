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

#ifndef PINCALLBACKS_HPP_
#define PINCALLBACKS_HPP_

#include "pin.H"
#include <pthread.h>

void setStartReached( THREADID tid );
void setEndReached( THREADID tid );

void startFunctionCall( THREADID tid, CONTEXT* ctxt );
void startBasicBlock( THREADID tid, CONTEXT* ctxt, UINT32 insnCount );
void memOp( THREADID tid, UINT32 pos, ADDRINT addr, UINT32 size, BOOL isRead,
            BOOL isStackRef );

extern AFUNPTR realPthreadSelf;
void threadBegin( THREADID tid, CONTEXT* ctxt, INT32 flags, VOID *v );
void threadEnd( THREADID tid, const CONTEXT* ctx, INT32 code, VOID *v );

void beforeMalloc( THREADID tid, ADDRINT size );
void afterMalloc( THREADID tid, ADDRINT pointer );
void beforeFree( THREADID tid, ADDRINT pointer );

void beforePthreadCreate( THREADID tid, pthread_t* pthreadT );
void afterPthreadCreate( THREADID tid );

void beforeLockAcquire( THREADID tid, ADDRINT lockAddr );
void afterLockAcquire( THREADID tid );
void beforeLockRelease( THREADID tid, ADDRINT lockAddr );
void beforeTrylock( THREADID tid, ADDRINT lockAddr );
void afterTrylock( THREADID tid, ADDRINT outcome );

void beforeJoin( THREADID tid, ADDRINT pthread_t );
void afterJoin( THREADID tid );

void beforeSignal( THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *from,
                   CONTEXT *to, INT32 info, VOID *v );
void beforeSyscall( THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD sys, VOID* unused );
void afterSyscall( THREADID tid, CONTEXT* ctx, SYSCALL_STANDARD sys, VOID* unused );

#endif /* PINCALLBACKS_HPP_ */
