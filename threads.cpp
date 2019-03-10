// 1. I have many troubles in pthread_exit because gardbage collection
//    So I modify that codes with refering sample codes
// 2. And I convert my codes into c++ for using queue library
// 3. I am learned of timer handling for reference source
//    ITIMER_REAL help making SIGALRM

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <setjmp.h> // setjmp, longjmp

#include <sys/time.h>
#include <time.h>
#include <signal.h> // ALARM signal related

#include <stdint.h> // uintptr_t

#include <string.h>
#include <queue>
#include <deque>
#include <iostream>
#include <pthread.h>

#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3
#define JB_SP 4
#define JB_PC 5

#define INTERVAL 50
#define STACK_SIZE 32767

using namespace std;
/*
 * ptr mangle ; for security reasons
 * just do mangle
 */
static int ptr_mangle(int p)
{
    unsigned int ret;
    __asm__(" movl %1, %%eax;\n"
        " xorl %%gs:0x18, %%eax;"
        " roll $0x9, %%eax;"
        " movl %%eax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%eax"
    );
    return ret;
}

template<typename T, typename Container=std::deque<T> >
class iterable_queue : public std::queue<T,Container>
{
public:
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

    iterator begin() { return this->c.begin(); }
    iterator end() { return this->c.end(); }
    const_iterator begin() const { return this->c.begin(); }
    const_iterator end() const { return this->c.end(); }
};


void tswitcher(int signo);
void gc_thread(void);

/* for signal blocking when switching thread */
sigset_t block_sig;

//static struct timeval tv1,tv2;
static struct itimerval interval_timer = {0};
static struct itimerval current_timer = {0};
static struct itimerval null_timer = {0};
static struct sigaction act;

/* 50 ms timer
 * This timer routine can start and stop SIGALRM
 * This control is much better than ualarm()
 */

static void pause_timer()
{
  // ITIMER_REAL can make SIGALRM
  // make current_timer null
  setitimer(ITIMER_REAL,&null_timer,&current_timer);
}

static void resume_timer()
{
  // ITIMER_REAL can make SIGALRM
  // makes current_timer active
  setitimer(ITIMER_REAL,&current_timer,NULL);
}

static void start_timer()
{
  // ITIMER_REAL can make SIGALRM
  // makes current_timer <- interval_timer 50ms
  current_timer = interval_timer; 
  setitimer(ITIMER_REAL,&current_timer,NULL);
}

static void stop_timer()
{
  // ITIMER_REAL can make SIGALRM
  // stop timer
  setitimer(ITIMER_REAL,&null_timer,NULL);
}


struct _thread {
  pthread_t id;
  jmp_buf env;
  char *stack;
  deque<struct _thread> wait_pool;
};
typedef struct _thread  THREAD; 

static deque<THREAD> thread_pool;

THREAD thread_pool_pop(pthread_t id)
{
for (auto it=thread_pool.begin(); it!=thread_pool.end();it++) {
  if (id==it->id) {
	thread_pool.erase(it);
	return *it;
  } // end if
} // end for
}

static THREAD init_thread;
static THREAD gc;

/* for assigning id */
static unsigned long id_counter = 1; 
/* we initialize in pthread_create only once */
static int pthread_initialized = 0;

/*
 * pthread_init()
 *
 * Initialize thread subsystem and scheduler
 * only called once, when first initializing timer/thread subsystem, etc... 
 */
void pthread_init() {
        // when alarm, do switch thread to another
  act.sa_handler = tswitcher;
  sigemptyset(&act.sa_mask); // signal masking
  act.sa_flags = SA_NODEFER; // for real time use

  if(sigaction(SIGALRM, &act, NULL) == -1) {
	printf("\nSIGALRM fail");
	exit(1);
  }

// for blocking signal
sigemptyset(&block_sig);
sigaddset(&block_sig, SIGALRM);


// itimer makes SIGALRM
interval_timer.it_value.tv_sec = INTERVAL/1000;
interval_timer.it_value.tv_usec = (INTERVAL*1000) % 1000000;
interval_timer.it_interval = interval_timer.it_value;

/* create thread control buffer for main thread, set as current active tcb */
	init_thread.id = 0;
	init_thread.stack = NULL;
	
	/* front of thread_pool is the active thread */
	thread_pool.push_back(init_thread);

	/* set up garbage collector */
	gc.id = 128;
	gc.stack = (char *) malloc (STACK_SIZE);

	/* initialize jump buf structure to be 0, just in case there's garbage */
	memset(&gc.env,0,sizeof(gc.env));
	/* the jmp buffer has a stored signal mask; zero it out just in case */
	sigemptyset(&gc.env->__saved_mask);

	/* garbage collector 'lives' in gc_thread */
	gc.env->__jmpbuf[JB_SP] = ptr_mangle((uintptr_t)(gc.stack+STACK_SIZE-8));
	gc.env->__jmpbuf[JB_PC] = ptr_mangle((uintptr_t)gc_thread);

	/* Initialize timer and wait for first sigalarm to go off */
	start_timer();
	pause();	
}



/* 
 * pthread_join()
 * 
 */
int pthread_join(pthread_t thread, void **value_ptr) 
{
 for (auto it=thread_pool.begin(); it!=thread_pool.end(); ++it) {
   if (it->id==thread) {
	printf("\njoin id: %d is attached to %d", pthread_self(), it->id);
	//thread_pool.erase(*it);
	//thread_pool.front().wait_pool.push_back(*it);
	it->wait_pool.push_back(thread_pool.front());
	printf("\nid(%d) is push_back to wait_pool", thread_pool.front().id);
	thread_pool.pop_front();
	break;
   }
 } // end for 
  //thread.wait_pool.push(pthread_self()); 
return 0;
}

/* 
 * pthread_create()
 * 
 */
int pthread_create(pthread_t *restrict_thread, const pthread_attr_t *restrict_attr, void *(*start_routine)(void*), void *restrict_arg) 
{
	
  /* set up thread subsystem and timer */
  if(!pthread_initialized) {
     pthread_initialized = 1;
     pthread_init();
  }

  pause_timer(); // for no intercept

  THREAD new_tcb;
  new_tcb.id = id_counter++;
  *restrict_thread = new_tcb.id;

  new_tcb.stack = (char *)malloc(STACK_SIZE);

  *(int*)(new_tcb.stack+STACK_SIZE-4) = (int)restrict_arg;
  // after function exit, execute pthread_exit()
  *(int*)(new_tcb.stack+STACK_SIZE-8) = (int)pthread_exit;
	
  memset(&new_tcb.env,0,sizeof(new_tcb.env));
  sigemptyset(&new_tcb.env->__saved_mask);
	
  new_tcb.env->__jmpbuf[JB_SP] = ptr_mangle((uintptr_t)(new_tcb.stack+STACK_SIZE-8));
  new_tcb.env->__jmpbuf[JB_PC] = ptr_mangle((uintptr_t)start_routine);

	/* new thread is ready to be scheduled! */
	thread_pool.push_back(new_tcb);
    
    /* resume timer */
    resume_timer();

    return 0;	
}



/* 
 * pthread_self()
 *
 * just return the current thread's id
 * undefined if thread has not yet been created
 * (e.g., main thread before setting up thread subsystem) 
 */
pthread_t pthread_self(void) {
	if(thread_pool.size() == 0) {
		return 0;
	} else {
		return (pthread_t)thread_pool.front().id;
	}
}

/* 
 * pthread_exit()
 *
 * pthread_exit will execute  after start_routine finishes
 */
void pthread_exit(void *value_ptr) {

if(pthread_initialized == 0) {
	exit(0);
}

/* for not geting interrupted */
stop_timer();

if(thread_pool.front().id == 0) {
  init_thread = thread_pool.front();
  if(setjmp(init_thread.env)) {
	free((void*) gc.stack);
	exit(0);
  } // end if 
} // end if
longjmp(gc.env,1); 
}

/* lock()
 * disable SIGALARM
 * not getting interrupted
 */
 
void lock()
{
  if (sigprocmask(SIG_BLOCK, &block_sig, NULL)!=0) {
     printf("\nlock() fail\n");
  }
}

/* unlock()
 * enable SIGALARM
 * getting interrupted
 */

void unlock()
{
  if (sigprocmask(SIG_UNBLOCK, &block_sig, NULL)!=0) {
     printf("\nlock() fail\n");
  }
}

void print_runqueue(deque<THREAD> tmp_thread_pool)
{
  deque<THREAD> copy_thread_pool;
  copy_thread_pool = tmp_thread_pool;
  printf("\nrunqueue: ");
  while (!copy_thread_pool.empty()) {
     printf("  %d", copy_thread_pool.front().id);
     copy_thread_pool.pop_front();
  }
  printf("\n");
}

/* 
 * tswitcher()
 * called when receiving SIGALRM
 */
void tswitcher(int signo) 
{ 
  if(thread_pool.size() <= 1) return;

  print_runqueue(thread_pool); 
	
  if(setjmp(thread_pool.front().env) == 0) { /* switch threads */
    // use setjmp as changing thread
    thread_pool.push_back(thread_pool.front());
    thread_pool.pop_front();
    longjmp(thread_pool.front().env,1);
  } // end if
  return;
}


/* 
 * gc_thread()
 * I experienced many toubles because of pthread_exit() 
 * This gc_thread helps solving pthread_exit() error
 */
void gc_thread(void)
{

  // free init 
  free((void*) thread_pool.front().stack);
  thread_pool.front().stack = NULL;
  printf("\n%d is deleted", thread_pool.front().id);
  // thread exited point
  // make waited thread live
  for (auto it=thread_pool.front().wait_pool.begin(); it!=thread_pool.front().wait_pool.end();it++) {
          thread_pool.push_back(*it);
	printf("\n%d is push_back to thread_pool", it->id);
  } // end for 

  /* Don't schedule the thread anymore */
  thread_pool.pop_front();

  if(thread_pool.size() == 0) { // no thread in pool mean this is last 
     longjmp(init_thread.env,1);
  } else {
     // will do rest thread
     start_timer();
     longjmp(thread_pool.front().env,1);
  } // end if
}

