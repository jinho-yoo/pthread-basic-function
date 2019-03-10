#define YOO_DEBUG 0
// for debug message

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>

/* example thread */
int g_count=0;

void *thread1(void *arg)
{

  while (1) {       
	printf("\nthread 1: %d(%d)", g_count, pthread_self());
	//printf("\ncurrent_thread_no 1: %d(%d)", current_thread_no, thread[current_thread_no].thread_id);
	
	g_count++;
	sleep(1);
  }
return NULL;
}

void *thread2(void *arg)
{
int count=10;
int first=1;
   while (count--) {       
	printf("\nthread 2: %d(%d)- count:%d", g_count, pthread_self(), count);
	g_count++;
#if 0
	if (first==0)
  	if (g_count>10) {
 		printf("\n FFFFFFFFFFFFFFFFFFF");
		lock();
		thread[1].status=THREAD_BLOCK;
		unlock();
		first=1;
	}
#endif
	sleep(1);
   }
return NULL;
}

void *thread3(void *arg)
{       
int first=0;
void **value_ptr;

   while (1) {       
	printf("\nthread 3: %d(%d)", g_count, pthread_self());
	g_count++;
	if (first==0)
  	if (g_count>20) {
 		printf("\n RRRRRRRRRRRRRRRRRRRR");
		pthread_join(2, value_ptr);
		first=1;
	}
	sleep(1);
   }
return NULL;
}

void force_sleep(int seconds) {
        struct timespec initial_spec, remainder_spec;
        initial_spec.tv_sec = (time_t)seconds;
        initial_spec.tv_nsec = 0;

        int err = -1;
        while(err == -1) {
                err = nanosleep(&initial_spec,&remainder_spec);
                initial_spec = remainder_spec;
                memset(&remainder_spec,0,sizeof(remainder_spec));
        }
}

int main(int argc, char **argv)
{
pthread_t t1;
pthread_t t2;
pthread_t t3;
int first=0;


pthread_create(&t1, NULL, thread1, NULL);
pthread_create(&t2, NULL, thread2, NULL);
pthread_create(&t3, NULL, thread3, NULL);

while (1) {
 printf("\n ----------- main while loop ----");
 fflush(stdout);
 
  if ((g_count>30) && (first==0)) {
		//lock();
		//delete_runqueue(1);
		//unlock();
	//pthread_exit(NULL);
	first=1;
  }
 
 sleep(1);
} // while

}
