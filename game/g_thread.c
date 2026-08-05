#include "g_thread.h"

#if defined(WIN32) || defined(__WIN32__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h> // _beginthreadex

// same size/alignment sanity checks the POSIX path has -- without them an oversized
// CRITICAL_SECTION/CONDITION_VARIABLE would silently overflow the opaque buffer on this target only
typedef char g_mutex_size_check[(sizeof(CRITICAL_SECTION) <= sizeof(((gmutex_t *)0)->u.opaque)) ? 1 : -1];
typedef char g_cond_size_check[(sizeof(CONDITION_VARIABLE) <= sizeof(((gcond_t *)0)->u.opaque)) ? 1 : -1];
typedef char g_mutex_align_check[(__alignof(gmutex_t) >= __alignof(CRITICAL_SECTION)) ? 1 : -1];
typedef char g_cond_align_check[(__alignof(gcond_t) >= __alignof(CONDITION_VARIABLE)) ? 1 : -1];

struct gthread_s {
	HANDLE handle;
};

typedef struct {
	gthreadFn_t fn;
	void *arg;
	qboolean lowerPriority;
	int niceAmount;
} winThreadStartArgs_t;

static unsigned __stdcall Win_ThreadTrampoline(void *rawArgs) {
	winThreadStartArgs_t args = *(winThreadStartArgs_t *)rawArgs;
	free(rawArgs);
	if (args.lowerPriority) {
		GThread_LowerCurrentThreadPriority(args.niceAmount);
	}
	args.fn(args.arg);
	return 0;
}

qboolean GThread_Create(gthread_t **outThread, gthreadFn_t fn, void *arg, qboolean lowerPriority, int niceAmount) {
	winThreadStartArgs_t *startArgs = (winThreadStartArgs_t *)malloc(sizeof(winThreadStartArgs_t));
	gthread_t *t;
	uintptr_t h;

	if (!startArgs)
		return qfalse;
	startArgs->fn = fn;
	startArgs->arg = arg;
	startArgs->lowerPriority = lowerPriority;
	startArgs->niceAmount = niceAmount;

	t = (gthread_t *)malloc(sizeof(gthread_t));
	if (!t) {
		free(startArgs);
		return qfalse;
	}

	h = _beginthreadex(NULL, 0, Win_ThreadTrampoline, startArgs, 0, NULL);
	if (!h) {
		free(t);
		free(startArgs);
		return qfalse;
	}

	t->handle = (HANDLE)h;
	*outThread = t;
	return qtrue;
}

void GThread_Join(gthread_t *thread) {
	if (!thread)
		return;
	WaitForSingleObject(thread->handle, INFINITE);
	CloseHandle(thread->handle);
	free(thread);
}

void GThread_LowerCurrentThreadPriority(int niceAmount) {
	// windows only exposes a handful of coarse priority levels, so map the niceAmount scale callers use
	// everywhere else onto the closest equivalents. higher niceAmount = more willing to yield, same
	// direction as POSIX nice.
	int amount = niceAmount > 0 ? niceAmount : 10;
	int priority;
	if (amount >= 19)
		priority = THREAD_PRIORITY_IDLE; // only runs when the system would otherwise be idle
	else if (amount >= 10)
		priority = THREAD_PRIORITY_LOWEST;
	else
		priority = THREAD_PRIORITY_BELOW_NORMAL;
	// best-effort only; failure is silently ignored
	SetThreadPriority(GetCurrentThread(), priority);
}

int GThread_HardwareConcurrency(void) {
	// prefer the process affinity mask over SYSTEM_INFO.dwNumberOfProcessors, same reason the POSIX path
	// prefers sched_getaffinity: that's how big the machine is, not how much of it we may use
	DWORD_PTR processMask = 0, systemMask = 0;
	if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask) {
		int n = 0;
		for (DWORD_PTR bit = processMask; bit; bit >>= 1)
			n += (int)(bit & 1);
		if (n >= 1)
			return n;
	}

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	if (si.dwNumberOfProcessors < 1)
		return 1;
	return (int)si.dwNumberOfProcessors;
}

int GThread_Milliseconds(void) {
	return (int)GetTickCount64();
}

void GMutex_Init(gmutex_t *m) {
	InitializeCriticalSection((CRITICAL_SECTION *)m->u.opaque);
}
void GMutex_Destroy(gmutex_t *m) {
	DeleteCriticalSection((CRITICAL_SECTION *)m->u.opaque);
}
void GMutex_Lock(gmutex_t *m) {
	EnterCriticalSection((CRITICAL_SECTION *)m->u.opaque);
}
void GMutex_Unlock(gmutex_t *m) {
	LeaveCriticalSection((CRITICAL_SECTION *)m->u.opaque);
}

void GCond_Init(gcond_t *c) {
	InitializeConditionVariable((CONDITION_VARIABLE *)c->u.opaque);
}
void GCond_Destroy(gcond_t *c) {
	// CONDITION_VARIABLE has no destroy on Win32
	(void)c;
}
void GCond_Wait(gcond_t *c, gmutex_t *m) {
	SleepConditionVariableCS((CONDITION_VARIABLE *)c->u.opaque, (CRITICAL_SECTION *)m->u.opaque, INFINITE);
}
void GCond_Signal(gcond_t *c) {
	WakeConditionVariable((CONDITION_VARIABLE *)c->u.opaque);
}
void GCond_Broadcast(gcond_t *c) {
	WakeAllConditionVariable((CONDITION_VARIABLE *)c->u.opaque);
}

int GAtomic_Get(volatile int *v) {
	return (int)InterlockedCompareExchange((volatile LONG *)v, 0, 0);
}
void GAtomic_Set(volatile int *v, int newVal) {
	InterlockedExchange((volatile LONG *)v, (LONG)newVal);
}
int GAtomic_IncRet(volatile int *v) {
	return (int)InterlockedIncrement((volatile LONG *)v);
}
qboolean GAtomic_CompareExchange(volatile int *v, int expected, int newVal) {
	return InterlockedCompareExchange((volatile LONG *)v, (LONG)newVal, (LONG)expected) == (LONG)expected ? qtrue : qfalse;
}

#else // POSIX (Linux x86 32-bit, ARM Linux 32-bit)

#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <errno.h>
#include <time.h>

// sanity: our opaque buffers must be large enough for the real primitive
typedef char g_mutex_size_check[(sizeof(pthread_mutex_t) <= sizeof(((gmutex_t *)0)->u.opaque)) ? 1 : -1];
typedef char g_cond_size_check[(sizeof(pthread_cond_t) <= sizeof(((gcond_t *)0)->u.opaque)) ? 1 : -1];
// ...and correctly aligned for it. this is the one that actually bites: satisfied on x86 regardless
// (unaligned atomics are merely slow there) but a misaligned pthread_cond_t faults on ARM.
typedef char g_mutex_align_check[(__alignof__(gmutex_t) >= __alignof__(pthread_mutex_t)) ? 1 : -1];
typedef char g_cond_align_check[(__alignof__(gcond_t) >= __alignof__(pthread_cond_t)) ? 1 : -1];

struct gthread_s {
	pthread_t handle;
};

typedef struct {
	gthreadFn_t fn;
	void *arg;
	qboolean lowerPriority;
	int niceAmount;
} posixThreadStartArgs_t;

static void *Posix_ThreadTrampoline(void *rawArgs) {
	posixThreadStartArgs_t args = *(posixThreadStartArgs_t *)rawArgs;
	free(rawArgs);
	if (args.lowerPriority) {
		GThread_LowerCurrentThreadPriority(args.niceAmount);
	}
	args.fn(args.arg);
	return NULL;
}

qboolean GThread_Create(gthread_t **outThread, gthreadFn_t fn, void *arg, qboolean lowerPriority, int niceAmount) {
	posixThreadStartArgs_t *startArgs = (posixThreadStartArgs_t *)malloc(sizeof(posixThreadStartArgs_t));
	gthread_t *t;

	if (!startArgs)
		return qfalse;
	startArgs->fn = fn;
	startArgs->arg = arg;
	startArgs->lowerPriority = lowerPriority;
	startArgs->niceAmount = niceAmount;

	t = (gthread_t *)malloc(sizeof(gthread_t));
	if (!t) {
		free(startArgs);
		return qfalse;
	}

	if (pthread_create(&t->handle, NULL, Posix_ThreadTrampoline, startArgs) != 0) {
		free(t);
		free(startArgs);
		return qfalse;
	}

	*outThread = t;
	return qtrue;
}

void GThread_Join(gthread_t *thread) {
	if (!thread)
		return;
	pthread_join(thread->handle, NULL);
	free(thread);
}

void GThread_LowerCurrentThreadPriority(int niceAmount) {
	// per-thread niceness on Linux: the "process id" for setpriority(PRIO_PROCESS, ...) is actually the
	// tid when called from within that thread. best-effort -- a failure here must never be fatal.
	pid_t tid = (pid_t)syscall(SYS_gettid);
	int amount = niceAmount > 0 ? niceAmount : 10;
	setpriority(PRIO_PROCESS, (id_t)tid, amount);
}

int GThread_HardwareConcurrency(void) {
	// sched_getaffinity, not sysconf(_SC_NPROCESSORS_ONLN): the latter reports how many CPUs the machine
	// has online and ignores anything restricting this process to fewer -- taskset, a cpuset cgroup, a
	// container's pinning. verified: under `taskset -c 0` sysconf still answers 16 on a 16-core host,
	// which would size the pool 15x too large.
	//
	// this still does NOT see a CFS quota (cgroup cpu.max, "2 CPUs' worth across all 16"), which no
	// affinity API exposes. on a quota-limited host set g_teamgen_workerThreads explicitly.
	cpu_set_t set;
	CPU_ZERO(&set);
	if (sched_getaffinity(0, sizeof(set), &set) == 0) {
		int n = CPU_COUNT(&set);
		if (n >= 1)
			return n;
	}

	long n = sysconf(_SC_NPROCESSORS_ONLN); // fallback if affinity is unavailable
	if (n < 1)
		return 1;
	return (int)n;
}

int GThread_Milliseconds(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	// (long long) first: tv_sec is a 32-bit time_t on the -m32/ARM targets, so the multiply overflows
	// (UB) after ~24.8 days of CLOCK_MONOTONIC uptime
	return (int)((long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000));
}

void GMutex_Init(gmutex_t *m) {
	pthread_mutex_init((pthread_mutex_t *)m->u.opaque, NULL);
}
void GMutex_Destroy(gmutex_t *m) {
	pthread_mutex_destroy((pthread_mutex_t *)m->u.opaque);
}
void GMutex_Lock(gmutex_t *m) {
	pthread_mutex_lock((pthread_mutex_t *)m->u.opaque);
}
void GMutex_Unlock(gmutex_t *m) {
	pthread_mutex_unlock((pthread_mutex_t *)m->u.opaque);
}

void GCond_Init(gcond_t *c) {
	pthread_cond_init((pthread_cond_t *)c->u.opaque, NULL);
}
void GCond_Destroy(gcond_t *c) {
	pthread_cond_destroy((pthread_cond_t *)c->u.opaque);
}
void GCond_Wait(gcond_t *c, gmutex_t *m) {
	pthread_cond_wait((pthread_cond_t *)c->u.opaque, (pthread_mutex_t *)m->u.opaque);
}
void GCond_Signal(gcond_t *c) {
	pthread_cond_signal((pthread_cond_t *)c->u.opaque);
}
void GCond_Broadcast(gcond_t *c) {
	pthread_cond_broadcast((pthread_cond_t *)c->u.opaque);
}

int GAtomic_Get(volatile int *v) {
	return __sync_fetch_and_add(v, 0);
}
void GAtomic_Set(volatile int *v, int newVal) {
	__sync_synchronize();
	*v = newVal;
	__sync_synchronize();
}
int GAtomic_IncRet(volatile int *v) {
	return __sync_add_and_fetch(v, 1);
}
qboolean GAtomic_CompareExchange(volatile int *v, int expected, int newVal) {
	return __sync_bool_compare_and_swap(v, expected, newVal) ? qtrue : qfalse;
}

#endif
