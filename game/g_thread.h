#ifndef G_THREAD_H
#define G_THREAD_H

#include "q_shared.h"

// minimal cross-platform thread / mutex / condvar / atomics shim for the async team-generation
// worker pool (see g_teamgen.c). pthreads on Linux/ARM, Win32 primitives on Windows. callers never
// see pthread_t/HANDLE, so this header pulls in neither <windows.h> nor <pthread.h>.
//
// all best-effort: if a primitive fails to init on some exotic host, callers should fall back to
// running synchronously rather than crash.

typedef struct gthread_s gthread_t;

// the union member is load-bearing, not decoration: a bare char[] has alignment 1, so an embedded
// gmutex_t/gcond_t could land on any byte offset. pthread_cond_t on arm-linux-gnueabihf needs
// 8-byte alignment, and a misaligned LDREX/STREX there is a data abort -- fine on x86, crashes on ARM.
typedef struct gmutex_s {
	union {
		char opaque[64];
		long long align_ll;
		void *align_ptr;
	} u;
} gmutex_t;

typedef struct gcond_s {
	union {
		char opaque[64];
		long long align_ll;
		void *align_ptr;
	} u;
} gcond_t;

// entry point signature for a worker thread body
typedef void (*gthreadFn_t)(void *arg);

// creates and starts a thread running fn(arg). qtrue on success, handle written to *outThread
// (caller must eventually GThread_Join it). lowerPriority makes the thread nice itself down by
// niceAmount shortly after starting (best-effort; failure ignored).
qboolean	GThread_Create(gthread_t **outThread, gthreadFn_t fn, void *arg, qboolean lowerPriority, int niceAmount);

// blocks until the thread finishes, then frees the handle. no-op if *thread is NULL.
void		GThread_Join(gthread_t *thread);

// called from inside a worker to lower its own scheduling priority. best-effort, never fatal.
void		GThread_LowerCurrentThreadPriority(int niceAmount);

// number of logical CPUs available to this process, or 1 if detection fails
int			GThread_HardwareConcurrency(void);

// monotonic wall-clock ms. unlike trap_Milliseconds(), which routes through the engine's syscall
// dispatcher and is main-thread only, this is safe from any thread. only meaningful for deltas --
// the raw values aren't on trap_Milliseconds()' epoch and must never be compared against it.
int			GThread_Milliseconds(void);

// mutex/condvar are currently unused -- teamgen spawns and joins per batch rather than parking
// threads on a condvar. kept for a future pool; create+join measured ~8.8us, so pooling wasn't worth it.
void		GMutex_Init(gmutex_t *m);
void		GMutex_Destroy(gmutex_t *m);
void		GMutex_Lock(gmutex_t *m);
void		GMutex_Unlock(gmutex_t *m);

void		GCond_Init(gcond_t *c);
void		GCond_Destroy(gcond_t *c);
// atomically unlocks m, waits for a signal/broadcast, then relocks m before returning
void		GCond_Wait(gcond_t *c, gmutex_t *m);
void		GCond_Signal(gcond_t *c);
void		GCond_Broadcast(gcond_t *c);

// 32-bit atomics with full barrier semantics (not just compiler-reordering protection) -- used for
// the job `done` flag, which is the release/acquire handoff publishing results to the main thread.
// don't rely on `volatile` alone for that. (abortRequested is set with GAtomic_Set but read with a
// plain volatile load in the worker hot loop -- see TeamGenWorker_Run for why.)
int			GAtomic_Get(volatile int *v);
void		GAtomic_Set(volatile int *v, int newVal);
int			GAtomic_IncRet(volatile int *v); // returns the post-increment value
qboolean	GAtomic_CompareExchange(volatile int *v, int expected, int newVal); // qtrue if swapped

#endif // G_THREAD_H
