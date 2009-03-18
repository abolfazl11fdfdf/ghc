/* ---------------------------------------------------------------------------
 *
 * (c) The GHC Team, 2001-2005
 *
 * Accessing OS threads functionality in a (mostly) OS-independent
 * manner. 
 *
 * --------------------------------------------------------------------------*/

#if defined(__linux__)
/* We want GNU extensions in DEBUG mode for mutex error checking */
/* We also want the affinity API, which requires _GNU_SOURCE */
#define _GNU_SOURCE
#endif

#include "Rts.h"
#if defined(THREADED_RTS)
#include "OSThreads.h"
#include "RtsUtils.h"
#include "Task.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#if !defined(HAVE_PTHREAD_H)
#error pthreads.h is required for the threaded RTS on Posix platforms
#endif

#if defined(HAVE_SCHED_H)
#include <sched.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/*
 * This (allegedly) OS threads independent layer was initially
 * abstracted away from code that used Pthreads, so the functions
 * provided here are mostly just wrappers to the Pthreads API.
 *
 */

void
initCondition( Condition* pCond )
{
  pthread_cond_init(pCond, NULL);
  return;
}

void
closeCondition( Condition* pCond )
{
  pthread_cond_destroy(pCond);
  return;
}

rtsBool
broadcastCondition ( Condition* pCond )
{
  return (pthread_cond_broadcast(pCond) == 0);
}

rtsBool
signalCondition ( Condition* pCond )
{
  return (pthread_cond_signal(pCond) == 0);
}

rtsBool
waitCondition ( Condition* pCond, Mutex* pMut )
{
  return (pthread_cond_wait(pCond,pMut) == 0);
}

void
yieldThread()
{
  sched_yield();
  return;
}

void
shutdownThread()
{
  pthread_exit(NULL);
}

int
createOSThread (OSThreadId* pId, OSThreadProc *startProc, void *param)
{
  int result = pthread_create(pId, NULL, (void *(*)(void *))startProc, param);
  if(!result)
    pthread_detach(*pId);
  return result;
}

OSThreadId
osThreadId()
{
  return pthread_self();
}

rtsBool
osThreadIsAlive(OSThreadId id STG_UNUSED)
{
    // no good way to implement this on POSIX, AFAICT.  Returning true
    // is safe.
    return rtsTrue;
}

void
initMutex(Mutex* pMut)
{
#if defined(DEBUG)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(pMut,&attr);
#else
    pthread_mutex_init(pMut,NULL);
#endif
    return;
}
void
closeMutex(Mutex* pMut)
{
    pthread_mutex_destroy(pMut);
}

void
newThreadLocalKey (ThreadLocalKey *key)
{
    int r;
    if ((r = pthread_key_create(key, NULL)) != 0) {
	barf("newThreadLocalKey: %s", strerror(r));
    }
}

void *
getThreadLocalVar (ThreadLocalKey *key)
{
    return pthread_getspecific(*key);
    // Note: a return value of NULL can indicate that either the key
    // is not valid, or the key is valid and the data value has not
    // yet been set.  We need to use the latter case, so we cannot
    // detect errors here.
}

void
setThreadLocalVar (ThreadLocalKey *key, void *value)
{
    int r;
    if ((r = pthread_setspecific(*key,value)) != 0) {
	barf("setThreadLocalVar: %s", strerror(r));
    }
}

void
freeThreadLocalKey (ThreadLocalKey *key)
{
    int r;
    if ((r = pthread_key_delete(*key)) != 0) {
	barf("freeThreadLocalKey: %s", strerror(r));
    }
}

static void *
forkOS_createThreadWrapper ( void * entry )
{
    Capability *cap;
    cap = rts_lock();
    cap = rts_evalStableIO(cap, (HsStablePtr) entry, NULL);
    taskTimeStamp(myTask());
    rts_unlock(cap);
    return NULL;
}

int
forkOS_createThread ( HsStablePtr entry )
{
    pthread_t tid;
    int result = pthread_create(&tid, NULL,
				forkOS_createThreadWrapper, (void*)entry);
    if(!result)
        pthread_detach(tid);
    return result;
}

nat
getNumberOfProcessors (void)
{
    static nat nproc = 0;

    if (nproc == 0) {
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
        nproc = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_CONF)
        nproc = sysconf(_SC_NPROCESSORS_CONF);
#else
        nproc = 1;
#endif
    }

    return nproc;
}

// Schedules the thread to run on CPU n of m.  m may be less than the
// number of physical CPUs, in which case, the thread will be allowed
// to run on CPU n, n+m, n+2m etc.
void
setThreadAffinity (nat n, nat m)
{
#if defined(HAVE_SCHED_H) && defined(HAVE_SCHED_SETAFFINITY)
    nat nproc;
    cpu_set_t cs;
    nat i;

    nproc = getNumberOfProcessors();
    CPU_ZERO(&cs);
    for (i = n; i < nproc; i+=m) {
        CPU_SET(n, &cs);
    }
    sched_setaffinity(0, sizeof(cpu_set_t), &cs);
#endif
}

#else /* !defined(THREADED_RTS) */

int
forkOS_createThread ( HsStablePtr entry STG_UNUSED )
{
    return -1;
}

#endif /* !defined(THREADED_RTS) */
