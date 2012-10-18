#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <map>
#include <stdio.h>

using namespace std;		

/* thread statuses*/
#define RUNNING_WITH_LOCK	1; //thread runs and does not need lock
#define RUNNING_NO_LOCK		2; //threads runs and is waiting for lock
#define TERMINATE 		3; //thread terminated

int (*original_pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) = NULL;
int (*original_pthread_join)(pthread_t, void**) = NULL;
int (*original_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;
int (*original_pthread_mutex_lock)(pthread_mutex_t*) = NULL;

static pthread_mutex_t	globalLock = PTHREAD_MUTEX_INITIALIZER; //global lock
static pthread_t	current = 0;


// PART 2
static int		total_sync_point = 0;
static int		current_sync_point = 1;
//static int		temp_global = 0;
static bool		wait = false;
static pthread_t tempthread;
static bool 		firsttime = true;


static map<pthread_mutex_t*, pthread_t> mutexMap; //maps mutex lock to its value
static map<pthread_t, int> threadMap; //maps thread to its status

static void initialize_original_functions();

static void performSyncPointCheck();
static bool isFirstExecution();
static int getCurrentSyncPoint();

struct Thread_Arg {
    void* (*start_routine)(void*);
    void* arg;
};

static
void* thread_main(void *arg)
{
    struct Thread_Arg thread_arg = *(struct Thread_Arg*)arg;
    free(arg);
        
	// spinlock
	//while(pthread_equal(pthread_self(),current) == 0) {}
	
	// lock the global lock
	original_pthread_mutex_lock(&globalLock);
	
    void* ret = thread_arg.start_routine(thread_arg.arg); //call second thread	
	// thread stops running
	threadMap[pthread_self()] = TERMINATE;
	
    original_pthread_mutex_unlock(&globalLock);

    /*PART 2*/

    // outputs the total synchronization points to the file syncpoints
    
    int temt;
    FILE *ftemp = fopen("syncpoints.txt","r");
    fscanf(ftemp,"%d",&temt);
    fclose(ftemp);
    
    if (temt == 0) {
		//fprintf(stderr,"updating sync points\n");
    	FILE *output = fopen("syncpoints.txt","w");
    	fprintf(output,"%d\n",total_sync_point);
    
    	fclose(output);
    }

    current = tempthread;
    return ret;
}

extern "C"
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg)
{
    initialize_original_functions();
	
    struct Thread_Arg *thread_arg = (struct Thread_Arg*)malloc(sizeof(struct Thread_Arg));
    thread_arg->start_routine = start_routine;
    thread_arg->arg = arg;
	
    // global lock is locked now (first instance of create a new thread)
	
	if (firsttime) {
		threadMap[pthread_self()] = RUNNING_WITH_LOCK;
		firsttime = false;
		current = pthread_self();	
		original_pthread_mutex_lock(&globalLock);
	}
	tempthread = pthread_self();

    int ret = original_pthread_create(thread, attr, thread_main, thread_arg);
	
    threadMap[*thread] = RUNNING_NO_LOCK;
	
	fprintf(stderr,"pthread_create\n");
    performSyncPointCheck();
	
    return ret;
}

extern "C"
int pthread_join(pthread_t joinee, void **retval)
{
    initialize_original_functions();
	original_pthread_mutex_unlock(&globalLock);
	
	
	
	/* select next available thread to execute */
	/* wait till joinee terminates */
	int x = TERMINATE;
	
	if (threadMap[joinee] != x)
		current = joinee;
	
	wait = true;

	//fprintf(stderr,"stuck in thread join :(\n");
	while(threadMap[joinee] != x) {}
	
	wait = false;
	
//	temp_global--;
	original_pthread_mutex_lock(&globalLock);	
	
    return original_pthread_join(joinee, retval);
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    initialize_original_functions();
	
	// check if *mutex is held by other threads.
	// but it needs to be in the map.
	
	// if mutex is held by other threads 
	// if thread in map is different from the current thread
	if (mutexMap[mutex] != 0)
	{
		// unlock the global lock first
		original_pthread_mutex_unlock(&globalLock);
			
		// select next available thread to execute; ie: find the next thread available to run
		
		current = mutexMap[mutex];
		threadMap[current] = RUNNING_WITH_LOCK;
		
		// wait till L is not held by other threads; ie wait till its done running
		while(pthread_equal(pthread_self(),current) == 0) {}
		
		// now the running thread is the other one
		current = pthread_self();
		
		original_pthread_mutex_lock(&globalLock);	
		
		// PART2 sync point - before mutex is locked
		fprintf(stderr,"lock1\n");
		performSyncPointCheck();
		
		// lock the mutex and change status of thread
		mutexMap[mutex] = current;
		threadMap[current] = RUNNING_WITH_LOCK;
	}
	else
	{		
		current = pthread_self();
		// PART2 sync point - before mutex is locked
		fprintf(stderr,"lock2\n");
		performSyncPointCheck();
		mutexMap[mutex] = current;
		threadMap[current] = RUNNING_WITH_LOCK;	
	}

    return original_pthread_mutex_lock(mutex);
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    initialize_original_functions();
	
	// if L belongs to current thread, then UNLOCK(L)
	if (pthread_equal(mutexMap[mutex],pthread_self()) != 0) {
		fprintf(stderr,"unlock\n");

		mutexMap[mutex] = 0;		

	}
	int ret = original_pthread_mutex_unlock(mutex);
	performSyncPointCheck();
    return ret;
}

extern "C"
int sched_yield(void)
{
	
//	fprintf(stderr,"in yield\n");
	original_pthread_mutex_unlock(&globalLock);

	// select next available thread to execute
	// if there's no other available thread, the calling thread will execute
	// if there's another available thread, the other thread should be executed
	
	map<pthread_t,int>::iterator thread_it;
	
	// iterate through each thread in the map
	for (thread_it = threadMap.begin() ; thread_it != threadMap.end() && !wait; thread_it++)
	{
		int x = RUNNING_NO_LOCK;
		// check if the thread is running with no lock
		if (thread_it->second == x) {
			// then let it run instead of the current thread
		//	threadMap[current] = RUNNING_NO_LOCK;
			current = thread_it->first;
			threadMap[current] = RUNNING_WITH_LOCK;
			break;
		}
	}
	while(pthread_equal(pthread_self(),current) == 0){}
	
	original_pthread_mutex_lock(&globalLock);
    return 0;
}

static
void initialize_original_functions()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        original_pthread_create =
            (int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*))dlsym(RTLD_NEXT, "pthread_create");
        original_pthread_join =
            (int (*)(pthread_t, void**))dlsym(RTLD_NEXT, "pthread_join");
        original_pthread_mutex_lock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_lock");
        original_pthread_mutex_unlock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    }
}

static
void performSyncPointCheck()
{
	// if its the first execution
	if (isFirstExecution())
	{
		// keep track of synchronization point
		total_sync_point++;	
	}
	else
	{
		// check if the current sync point in the file is the same as the one in the program 
		if (getCurrentSyncPoint() == current_sync_point++) {
			//switchThread();
			sched_yield();
			fprintf(stderr,"yielded");
		}
		//current_sync_point++;			
	}
}


// checks if the current execution is the first execution
static bool isFirstExecution()
{
	int temp;
	FILE *f = fopen("syncpoints.txt","r");
	fscanf(f,"%d",&temp);
	fclose(f);
	if (temp == 0) return true;
	else return false;
}

// get the current sync point from the text file
static int getCurrentSyncPoint()
{
	int temp;
	FILE *f = fopen("nth-thread.txt","r");
	fscanf(f,"%d",&temp);
	fclose(f);
	return temp;	
}
