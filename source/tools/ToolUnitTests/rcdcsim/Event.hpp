#ifndef EVENT_HPP_
#define EVENT_HPP_

#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <string>

using namespace std;

#include <assert.h>
#include <stdint.h>

enum EventType {
  INVALID_EVENT,
  ROI_START, ROI_FINISH, /** Tracking the Parsec region-of-interest */
  THREAD_START, /** When a new thread starts running, so we can setup its resources */
  THREAD_FINISH, /** When a thread exits, so we can teardown its resources */
  THREAD_BLOCKED, /** When a thread is about to block inside the kernel, e.g. when doing a join */
  THREAD_UNBLOCKED, /** When a thread returns from blocking inside the kernel, e.g. after having joined */
  MEMORY_READ, MEMORY_WRITE,
  MEMORY_ALLOCATION, MEMORY_FREE,
  BASIC_BLOCK, /** Insn counting */
  HAPPENS_BEFORE_SOURCE, HAPPENS_BEFORE_SINK /** All sync ops are mapped down to HB edges */
};

class Event {
#ifdef SIMULATOR_FRONTEND
private:
  /** Which thread last accessed each sync object. Used to provide source
   * information on HB-sink events. */
  static map<ADDRINT, THREADID> s_WhoLastAccessed;
  /** Lock for s_WhoLastAccessed */
  static PinLock sl_LastAccessed;
#endif

public:
  EventType m_type;
  uint16_t m_tid;

  // MEMORY_READ, MEMORY_WRITE, MEMORY_ALLOCATION, MEMORY_FREE
  uint64_t m_addr;
  uint8_t m_memOpSize;
  bool m_stackRef;

  // HAPPENS_BEFORE_SOURCE, HAPPENS_BEFORE_SINK
  uint64_t m_syncObject;
  uint64_t m_logicalTime; /** filled in by the simulator */
  bool m_isLifeLock;

  /** The thread that was the source of this HB-sink event. INVALID_THREADID if
  there is no such thread, e.g. the very first time this lock is acquired. */
  uint16_t m_hbSourceThread; // HAPPENS_BEFORE_SINK

  uint8_t m_insnCount; // BASIC_BLOCK

#ifdef SIMULATOR_FRONTEND
  static Event MemoryEvent( unsigned tid, EventType typ, uint64_t addr,
                            unsigned memOpSize, bool stackRef ) {
    assert( MEMORY_READ == typ || MEMORY_WRITE == typ );
    Event e = Event( tid, typ );
    e.m_addr = addr;
    e.m_memOpSize = memOpSize;
    e.m_stackRef = stackRef;
    return e;
  }

  static Event AllocationEvent( unsigned tid, EventType typ, uint64_t startAddr, uint64_t extent=0) {
    assert( MEMORY_ALLOCATION == typ || MEMORY_FREE == typ );
    if ( 0 == extent ) {
      assert( MEMORY_FREE == typ );
    }
    Event e = Event( tid, typ );
    e.m_addr = startAddr;
    e.m_memOpSize = extent;
    return e;
  }

  static Event SyncSourceEvent( unsigned tid, EventType typ, uint64_t syncObject, bool lifelock ) {
    assert( HAPPENS_BEFORE_SOURCE == typ );
    Event e = Event( tid, typ );
    e.m_syncObject = syncObject;
    e.m_isLifeLock = lifelock;

    sl_LastAccessed.lock();
    s_WhoLastAccessed[syncObject] = tid;
    sl_LastAccessed.unlock();

    return e;
  }

  static Event SyncSinkEvent( unsigned tid, EventType typ, uint64_t syncObject, bool lifelock ) {
    assert( HAPPENS_BEFORE_SINK == typ );
    Event e = Event( tid, typ );
    e.m_syncObject = syncObject;
    e.m_isLifeLock = lifelock;

    THREADID lastAccessor = INVALID_THREADID;
    sl_LastAccessed.lock();
    if ( s_WhoLastAccessed.count(syncObject) > 0 ) {
      lastAccessor = s_WhoLastAccessed.at( syncObject );
    }
    sl_LastAccessed.unlock();

    e.m_hbSourceThread = lastAccessor;
    return e;
  }

  static Event BasicBlockEvent( unsigned tid, EventType typ, unsigned insnCount ) {
    assert( BASIC_BLOCK == typ );
    Event e = Event( tid, typ );
    e.m_insnCount = insnCount;
    return e;
  }

  static Event ThreadEvent( unsigned tid, EventType typ ) {
    assert( ROI_START == typ || ROI_FINISH == typ || THREAD_START == typ ||
            THREAD_FINISH == typ || THREAD_BLOCKED == typ || THREAD_UNBLOCKED == typ );
    return Event( tid, typ );
  }
#endif

  /** This no-arg ctor is needed only for the lockfree ringbuffer. */
  Event() {
    clear();
  }

  /** reset this Event */
  void clear() {
    m_type = INVALID_EVENT;
    m_tid = -1;
    m_addr = 0;
    m_memOpSize = 0;
    m_stackRef = false;
    m_syncObject = 0;
    m_hbSourceThread = -1;
    m_insnCount = 0;
    m_logicalTime = 0;
    m_isLifeLock = false;
  }

  string toString() {
    string name;
    stringstream ss;

    switch ( m_type ) {
    case INVALID_EVENT:
      ss << "INVALID event";
      ss << " tid=" << m_tid;
      ss << " addr=" << m_addr;
      ss << " size=" << m_memOpSize;
      ss << " stack=" << m_stackRef;
      ss << " syncObj=" << m_syncObject;
      ss << " sourceTid=" << m_hbSourceThread;
      ss << " icount=" << m_insnCount;
      break;

    case ROI_START:
      name = "ROI_start";
      goto PrintThreadEvent;
    case ROI_FINISH:
      name = "ROI_finish";
      goto PrintThreadEvent;
    case THREAD_BLOCKED:
      name = "thread_blocked";
      goto PrintThreadEvent;
    case THREAD_UNBLOCKED:
      name = "thread_unblocked";
      goto PrintThreadEvent;
    case THREAD_START:
      name = "thread_start";
      goto PrintThreadEvent;
    case THREAD_FINISH:
      name = "thread_finish";
      PrintThreadEvent: ss << name << ", tid=" << m_tid;
      break;

    case MEMORY_READ:
      name = "read";
      goto PrintMemEvent;
    case MEMORY_WRITE:
      name = "write";
      PrintMemEvent: ss << name << ", tid=" << m_tid << ", size=" << m_memOpSize
          << ", stack=" << m_stackRef;
      break;

    case MEMORY_ALLOCATION:
      name = "alloc";
      goto PrintAllocEvent;
    case MEMORY_FREE:
      name = "free";
      PrintAllocEvent: ss << name << ", tid=" << m_tid << ", addr=0x" << hex << m_addr << dec << ", size=" << m_memOpSize ;
      break;

    case BASIC_BLOCK:
      ss << "basicblock" << ", tid=" << m_tid << ", insnCount=" << m_insnCount;
      break;

    case HAPPENS_BEFORE_SOURCE:
      name = "HB_source";
      goto PrintSyncEvent;
    case HAPPENS_BEFORE_SINK:
      name = "HB_sink";
      PrintSyncEvent: ss << name << ", tid=" << m_tid << ", syncObject=0x" << hex << m_syncObject << dec;
      if ( m_hbSourceThread != INVALID_THREADID ) {
        ss << ", hbSourceTid=" << m_hbSourceThread;
      }
      break;

    default:
      assert(false);
    }

    return ss.str();
  }

private:
  /** Private ctor */
  Event( unsigned tid, EventType typ ) {
    clear();
    m_tid = tid;
    m_type = typ;
  }

};

#endif /* EVENT_HPP_ */
