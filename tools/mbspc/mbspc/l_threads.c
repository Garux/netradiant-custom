/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "l_cmd.h"
#include "l_threads.h"
#include "l_log.h"
#include "l_mem.h"

#define	MAX_THREADS	64

//#define THREAD_DEBUG

int dispatch;
int workcount;
int oldf;
qboolean pacifier;
qboolean	threaded;
void (*workfunction) (int);

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int GetThreadWork(void)
{
	int	r;
	int	f;

	ThreadLock();

	if (dispatch == workcount)
	{
		ThreadUnlock ();
		return -1;
	}

	f = 10*dispatch / workcount;
	if (f != oldf)
	{
		oldf = f;
		if (pacifier)
			printf ("%i...", f);
	} //end if

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
} //end of the function GetThreadWork
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadWorkerFunction(int threadnum)
{
	int		work;

	while(1)
	{
		work = GetThreadWork ();
		if (work == -1)
			break;
//printf ("thread %i, work %i\n", threadnum, work);
		workfunction(work);
	} //end while
} //end of the function ThreadWorkerFunction
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(int))
{
	if (numthreads == -1)
		ThreadSetDefault ();
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
} //end of the function RunThreadsOnIndividual


//===================================================================
//
// Win32
//
//===================================================================

#if defined _WIN32              // ******************************************************

#define MULTI_THREAD

#include <windows.h>

typedef struct thread_s
{
	HANDLE handle;
	int threadid;
	DWORD id;
	struct thread_s *next;
} thread_t;

thread_t *firstthread;
thread_t *lastthread;
int currentnumthreads;
int currentthreadid;

int numthreads = 1;
CRITICAL_SECTION crit;
HANDLE semaphore;
static int enter;
static int numwaitingthreads = 0;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetDefault(void)
{
	SYSTEM_INFO info;

	if (numthreads == -1)	// not set manually
	{
		GetSystemInfo (&info);
		numthreads = info.dwNumberOfProcessors;
		if (numthreads < 1 || numthreads > 32)
			numthreads = 1;
	} //end if
	qprintf ("%i threads\n", numthreads);
} //end of the function ThreadSetDefault
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadLock(void)
{
	if (!threaded)
	{
		Error("ThreadLock: !threaded");
		return;
	} //end if
	EnterCriticalSection(&crit);
	if (enter)
		Error("Recursive ThreadLock\n");
	enter = 1;
} //end of the function ThreadLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadUnlock (void)
{
	if (!threaded)
	{
		Error("ThreadUnlock: !threaded");
		return;
	} //end if
	if (!enter)
		Error("ThreadUnlock without lock\n");
	enter = 0;
	LeaveCriticalSection(&crit);
} //end of the function ThreadUnlock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetupLock(void)
{
	Log_Print("Win32 multi-threading\n");
	InitializeCriticalSection(&crit);
	threaded = true;	//Stupid me... forgot this!!!
	currentnumthreads = 0;
	currentthreadid = 0;
} //end of the function ThreadInitLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownLock(void)
{
	DeleteCriticalSection(&crit);
	threaded = false;	//Stupid me... forgot this!!!
} //end of the function ThreadShutdownLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetupSemaphore(void)
{
	semaphore = CreateSemaphore(NULL, 0, 99999999, "bspc");
} //end of the function ThreadSetupSemaphore
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownSemaphore(void)
{
} //end of the function ThreadShutdownSemaphore
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreWait(void)
{
	WaitForSingleObject(semaphore, INFINITE);
} //end of the function ThreadSemaphoreWait
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreIncrease(int count)
{
	ReleaseSemaphore(semaphore, count, NULL);
} //end of the function ThreadSemaphoreIncrease
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RunThreadsOn(int workcnt, qboolean showpacifier, void(*func)(int))
{
//  int threadid[MAX_THREADS];
	DWORD threadid[MAX_THREADS];  // ML081030 modified
	HANDLE	threadhandle[MAX_THREADS];
	int		i;
	int		start, end;

	Log_Print("Win32 multi-threading\n");
	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = true;

	if (numthreads == -1)
		ThreadSetDefault ();

	if (numthreads < 1 || numthreads > MAX_THREADS) numthreads = 1;
	//
	// run threads in parallel
	//
	InitializeCriticalSection (&crit);

	numwaitingthreads = 0;

	if (numthreads == 1)
	{	// use same thread
		func (0);
	} //end if
	else
	{
//		printf("starting %d threads\n", numthreads);
		for (i = 0; i < numthreads; i++)
		{
			threadhandle[i] = CreateThread(
			   NULL,	// LPSECURITY_ATTRIBUTES lpsa,
			   0,		// DWORD cbStack,
			   (LPTHREAD_START_ROUTINE)func,	// LPTHREAD_START_ROUTINE lpStartAddr,
			   (LPVOID)i,	// LPVOID lpvThreadParm,
			   0,			//   DWORD fdwCreate,
			   &threadid[i]);
//			printf("started thread %d\n", i);
		} //end for

		for (i = 0; i < numthreads; i++)
			WaitForSingleObject (threadhandle[i], INFINITE);
	} //end else
	DeleteCriticalSection (&crit);

	threaded = false;
	end = I_FloatTime ();
	if (pacifier) printf (" (%i)\n", end-start);
} //end of the function RunThreadsOn
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AddThread(void (*func)(int))
{
	thread_t *thread;

	if (numthreads == 1)
	{
		if (currentnumthreads >= numthreads) return;
		currentnumthreads++;
		func(-1);
		currentnumthreads--;
	} //end if
	else
	{
		ThreadLock();
		if (currentnumthreads >= numthreads)
		{
			ThreadUnlock();
			return;
		} //end if
		//allocate new thread
		thread = GetMemory(sizeof(thread_t));
		if (!thread) Error("can't allocate memory for thread\n");

		//
		thread->threadid = currentthreadid;
		thread->handle = CreateThread(
				   NULL,	// LPSECURITY_ATTRIBUTES lpsa,
				   0,		// DWORD cbStack,
				   (LPTHREAD_START_ROUTINE)func,	// LPTHREAD_START_ROUTINE lpStartAddr,
				   (LPVOID) thread->threadid,			// LPVOID lpvThreadParm,
					0,						// DWORD fdwCreate,
					&thread->id);

		//add the thread to the end of the list
		thread->next = NULL;
		if (lastthread) lastthread->next = thread;
		else firstthread = thread;
		lastthread = thread;
		//
#ifdef THREAD_DEBUG
		qprintf("added thread with id %d\n", thread->threadid);
#endif //THREAD_DEBUG
		//
		currentnumthreads++;
		currentthreadid++;
		//
		ThreadUnlock();
	} //end else
} //end of the function AddThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RemoveThread(int threadid)
{
	thread_t *thread, *last;

	//if a single thread
	if (threadid == -1) return;
	//
	ThreadLock();
	last = NULL;
	for (thread = firstthread; thread; thread = thread->next)
	{
		if (thread->threadid == threadid)
		{
			if (last) last->next = thread->next;
			else firstthread = thread->next;
			if (!thread->next) lastthread = last;
			//
			FreeMemory(thread);
			currentnumthreads--;
#ifdef THREAD_DEBUG
			qprintf("removed thread with id %d\n", threadid);
#endif //THREAD_DEBUG
			break;
		} //end if
		last = thread;
	} //end if
	if (!thread) Error("couldn't find thread with id %d", threadid);
	ThreadUnlock();
} //end of the function RemoveThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void WaitForAllThreadsFinished(void)
{
	HANDLE handle;

	ThreadLock();
	while(firstthread)
	{
		handle = firstthread->handle;
		ThreadUnlock();

		WaitForSingleObject(handle, INFINITE);

		ThreadLock();
	} //end while
	ThreadUnlock();
} //end of the function WaitForAllThreadsFinished
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int GetNumThreads(void)
{
	return currentnumthreads;
} //end of the function GetNumThreads

#endif // _WIN32 ***********************************************************




//===================================================================
//
// LINUX
//
//===================================================================

#if defined(LINUX)

#define	MULTI_THREAD

#include <pthread.h>
#include <semaphore.h>

typedef struct thread_s
{
	pthread_t thread;
	int threadid;
	int id;
	struct thread_s *next;
} thread_t;

thread_t *firstthread;
thread_t *lastthread;
int currentnumthreads;
int currentthreadid;

int numthreads = 1;
pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t	attrib;
sem_t semaphore;
static int enter;


//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetDefault(void)
{
	if (numthreads == -1)	// not set manually
	{
		numthreads = 1;
	} //end if
	qprintf("%i threads\n", numthreads);
} //end of the function ThreadSetDefault
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadLock(void)
{
	if (!threaded)
	{
		Error("ThreadLock: !threaded");
		return;
	} //end if
	pthread_mutex_lock(&my_mutex);
	if (enter)
		Error("Recursive ThreadLock\n");
	enter = 1;
} //end of the function ThreadLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadUnlock(void)
{
	if (!threaded)
	{
		Error("ThreadUnlock: !threaded");
		return;
	} //end if
	if (!enter)
		Error("ThreadUnlock without lock\n");
	enter = 0;
	pthread_mutex_unlock(&my_mutex);
} //end of the function ThreadUnlock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetupLock(void)
{
	Log_Print("pthread multi-threading\n");

	threaded = true;
	currentnumthreads = 0;
	currentthreadid = 0;
} //end of the function ThreadInitLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownLock(void)
{
	threaded = false;
} //end of the function ThreadShutdownLock
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadSetupSemaphore(void)
{
	sem_init(&semaphore, 0, 0);
} //end of the function ThreadSetupSemaphore
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownSemaphore(void)
{
	sem_destroy(&semaphore);
} //end of the function ThreadShutdownSemaphore
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreWait(void)
{
	sem_wait(&semaphore);
} //end of the function ThreadSemaphoreWait
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreIncrease(int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		sem_post(&semaphore);
	} //end for
} //end of the function ThreadSemaphoreIncrease
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RunThreadsOn(int workcnt, qboolean showpacifier, void(*func)(int))
{
	int		i;
	pthread_t	work_threads[MAX_THREADS];
	void *pthread_return;
	int		start, end;

	Log_Print("pthread multi-threading\n");

	start = I_FloatTime ();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = true;

	if (numthreads < 1 || numthreads > MAX_THREADS) numthreads = 1;

	if (pacifier)
		setbuf (stdout, NULL);

	for (i=0 ; i<numthreads ; i++)
	{
		if (pthread_create(&work_threads[i], NULL, (void *)func, (void *)i) == -1)
			Error ("pthread_create failed");
	}

	for (i=0 ; i<numthreads ; i++)
	{
		if (pthread_join(work_threads[i], &pthread_return) == -1)
			Error ("pthread_join failed");
	}

	threaded = false;

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
} //end of the function RunThreadsOn
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AddThread(void (*func)(int))
{
	thread_t *thread;

	if (numthreads == 1)
	{
		if (currentnumthreads >= numthreads) return;
		currentnumthreads++;
		func(-1);
		currentnumthreads--;
	} //end if
	else
	{
		ThreadLock();
		if (currentnumthreads >= numthreads)
		{
			ThreadUnlock();
			return;
		} //end if
		//allocate new thread
		thread = GetMemory(sizeof(thread_t));
		if (!thread) Error("can't allocate memory for thread\n");
		//
		thread->threadid = currentthreadid;

		if (pthread_create(&thread->thread, NULL, (void *)func, (void *)thread->threadid) == -1)
			Error ("pthread_create failed");

		//add the thread to the end of the list
		thread->next = NULL;
		if (lastthread) lastthread->next = thread;
		else firstthread = thread;
		lastthread = thread;
		//
#ifdef THREAD_DEBUG
		qprintf("added thread with id %d\n", thread->threadid);
#endif //THREAD_DEBUG
		//
		currentnumthreads++;
		currentthreadid++;
		//
		ThreadUnlock();
	} //end else
} //end of the function AddThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RemoveThread(int threadid)
{
	thread_t *thread, *last;

	//if a single thread
	if (threadid == -1) return;
	//
	ThreadLock();
	last = NULL;
	for (thread = firstthread; thread; thread = thread->next)
	{
		if (thread->threadid == threadid)
		{
			if (last) last->next = thread->next;
			else firstthread = thread->next;
			if (!thread->next) lastthread = last;
			//
			FreeMemory(thread);
			currentnumthreads--;
#ifdef THREAD_DEBUG
			qprintf("removed thread with id %d\n", threadid);
#endif //THREAD_DEBUG
			break;
		} //end if
		last = thread;
	} //end if
	if (!thread) Error("couldn't find thread with id %d", threadid);
	ThreadUnlock();
} //end of the function RemoveThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void WaitForAllThreadsFinished(void)
{
	pthread_t *thread;
	void *pthread_return;

	ThreadLock();
	while(firstthread)
	{
		thread = &firstthread->thread;
		ThreadUnlock();

		if (pthread_join(*thread, &pthread_return) == -1)
			Error("pthread_join failed");

		ThreadLock();
	} //end while
	ThreadUnlock();
} //end of the function WaitForAllThreadsFinished
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int GetNumThreads(void)
{
	return currentnumthreads;
} //end of the function GetNumThreads

#endif //LINUX





//=======================================================================
//
// SINGLE THREAD
//
//=======================================================================

#if ! defined MULTI_THREAD

int numthreads = 1;
int currentnumthreads = 0;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetDefault(void)
{
	numthreads = 1;
} //end of the function ThreadSetDefault
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadLock(void)
{
} //end of the function ThreadLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadUnlock(void)
{
} //end of the function ThreadUnlock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetupLock(void)
{
	Log_Print("no multi-threading\n");
} //end of the function ThreadInitLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownLock(void)
{
} //end of the function ThreadShutdownLock
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSetupSemaphore(void)
{
} //end of the function ThreadSetupSemaphore
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void ThreadShutdownSemaphore(void)
{
} //end of the function ThreadShutdownSemaphore
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreWait(void)
{
} //end of the function ThreadSemaphoreWait
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void ThreadSemaphoreIncrease(int count)
{
} //end of the function ThreadSemaphoreIncrease
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RunThreadsOn(int workcnt, qboolean showpacifier, void(*func)(int))
{
	int start, end;

	Log_Print("no multi-threading\n");
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	start = I_FloatTime ();
	func(0);

	end = I_FloatTime ();
	if (pacifier)
		printf (" (%i)\n", end-start);
} //end of the function RunThreadsOn
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AddThread(void (*func)(int))
{
	if (currentnumthreads >= numthreads) return;
	currentnumthreads++;
	func(-1);
	currentnumthreads--;
} //end of the function AddThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void RemoveThread(int threadid)
{
} //end of the function RemoveThread
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void WaitForAllThreadsFinished(void)
{
} //end of the function WaitForAllThreadsFinished
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int GetNumThreads(void)
{
	return currentnumthreads;
} //end of the function GetNumThreads

#endif // MULTI_THREAD
