// WigWag LLC
// (c) 2011
// test_tw_sema.cpp
// Author: ed
// Mar 22, 2011/*
// Mar 22, 2011 * test_tw_sema.cpp
// Mar 22, 2011 *
// Mar 22, 2011 *  Created on: Mar 22, 2011
// Mar 22, 2011 *      Author: ed
// Mar 22, 2011 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include <utility>

#include <TW/tw_utils.h>
#include <TW/tw_alloc.h>
#include <TW/tw_sema.h>
#include <TW/tw_circular.h>


void *print_message_function( void *ptr );

#define QUEUE_SIZE 20
#define RUN_SIZE 200

#define CONSUMER_THREADS 4
#define PRODUCER_THREADS 2


#define START_VAL 0

int TOTAL = RUN_SIZE;
TW_Mutex *totalMutex;

/*
struct timeval* usec_to_timeval( int64_t usec, struct timeval* tv )
{
	tv->tv_sec = usec / 1000000 ;
	tv->tv_usec = usec % 1000000 ;
	return tv ;
}

struct timeval* add_usec_to_timeval( int64_t usec, struct timeval* tv ) {
    tv->tv_sec += usec / 1000000 ;
    tv->tv_usec += usec % 1000000 ;

//    tv.tv_sec = tv1.tv_sec + tv2.tv_sec ;  // add seconds
 //   tv.usec = tv1.tv_usec + tv2.tv_usec ; // add microseconds
//    tv->tv_sec += tv.tv_usec / 1000000 ;  // add microsecond overflow to seconds
//   tv->tv_usec %= 1000000 ; // subtract the overflow from microseconds
    return tv;
}

*/

using namespace TWlib;

typedef Allocator<Alloc_Std> TESTAlloc;

int OUTPUT[PRODUCER_THREADS];

class threadinfo {
public:
	int threadnum;
	void *p; // some data
};

class data {
public:
	int x;
	data() : x(0) {}
	data(data &) = delete;
	data(data &&o) : x(o.x) { o.x = 0; }
	data& operator=(data&& other) {
	     x = other.x;
	     other.x = 0;
	     return *this;
	}
};

void *producer( void *ptr ) {
	threadinfo *inf = reinterpret_cast<threadinfo *>(ptr);
	tw_safeCircular<data, TESTAlloc > *Q = reinterpret_cast<tw_safeCircular<data, TESTAlloc > *>(inf->p);
	int x = RUN_SIZE / PRODUCER_THREADS;
	int val = START_VAL;
	data D;
	while(x > 0) {
		val++;
		x--;
		printf(">>> Producer %d: adding %d\n\n", inf->threadnum, val);
		D.x = val;
		Q->addMv(D);
	}
}

void *consumer( void *ptr ) {
	threadinfo *inf = reinterpret_cast<threadinfo *>(ptr);
	tw_safeCircular<data, TESTAlloc > *Q = reinterpret_cast<tw_safeCircular<data, TESTAlloc > *>(inf->p);
	int x = RUN_SIZE / CONSUMER_THREADS;
	int cnt = 0;
	int tc = 0;
	data D;
	while(x > 0) {
		// this also works...
//		totalMutex->acquire();
//		printf("HERE\n");
//		if(TOTAL < 1) {
//			totalMutex->release();
//			break;
//		}
		if(Q->removeMvOrBlock(D)) {
			cnt++;
			printf("<<< Consumer %d: removed %d\n\n",inf->threadnum, D.x);
			TOTAL--;
		} else {
			printf("<<< Consumer %d: error - bad remove\n\n", inf->threadnum);
		}
//		totalMutex->release();
		x--;
	}
	printf("Consumer %d done!!!\n", inf->threadnum);
	OUTPUT[inf->threadnum] = cnt;
}



int main()
{
     //pthread_t thread1, thread2, thread3;
	pthread_t consumert[CONSUMER_THREADS];
	pthread_t producert[PRODUCER_THREADS];


	for (int x=0;x<CONSUMER_THREADS;x++)
		OUTPUT[x] = 0;

     int  iret1, iret2, iret3;
     totalMutex = new TW_Mutex();


     tw_safeCircular<data, TESTAlloc > theQ( QUEUE_SIZE, true );

    /* Create independent threads each of which will execute function */
	 threadinfo *inf;

     for (int x=0;x<CONSUMER_THREADS;x++) {
    	 inf = new threadinfo;
    	 inf->p = reinterpret_cast<void *>(&theQ);
    	 inf->threadnum = x;
    	 pthread_create( &consumert[x], NULL, consumer, reinterpret_cast<void *>(inf));
     }

     for (int x=0;x<PRODUCER_THREADS;x++) {
    	 inf = new threadinfo;
    	 inf->p = reinterpret_cast<void *>(&theQ);
    	 inf->threadnum = x;
    	 pthread_create( &producert[x], NULL, producer, reinterpret_cast<void *>(inf));
     }


//     iret1 = pthread_create( &thread1, NULL, creator, reinterpret_cast<void *>(&theQ));
//     iret2 = pthread_create( &thread2, NULL, consumer, reinterpret_cast<void *>(&theQ));

  //   iret3 = pthread_create( &thread3, NULL, print_and_look, reinterpret_cast<void *>(&test_sema));

     /* Wait till threads are complete before main continues. Unless we  */
     /* wait we run the risk of executing an exit which will terminate   */
     /* the process and all threads before the threads have completed.   */


     for (int x=0;x<CONSUMER_THREADS;x++) {
    	 pthread_join( consumert[x], NULL);
     }
     for (int x=0;x<PRODUCER_THREADS;x++) {
    	 pthread_join( producert[x], NULL);
     }

//     pthread_join( thread2, NULL);
//     pthread_join( thread3, NULL);

 //    printf("Thread 1 returns: %d\n",iret1);
  //   printf("Thread 2 returns: %d\n",iret2);

 	for (int x=0;x<CONSUMER_THREADS;x++) {
 		printf("Thread num %d -> output %d\n", x, OUTPUT[x]);
 	}



     exit(0);
}

