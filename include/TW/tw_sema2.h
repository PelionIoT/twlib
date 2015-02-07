// WigWag LLC
// (c) 2011
// tw_sema.h
// Author: ed
// Mar 22, 2011

/*
 * tw_sema.h
 *
 *  Created on: Mar 22, 2011
 *      Author: ed
 */

#ifndef TW_SEMA2_H_
#define TW_SEMA2_H_

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include <TW/tw_utils.h>

//#define _TW_SEMA_HEAVY_DEBUG

#ifdef _TW_SEMA_HEAVY_DEBUG
#include <stdio.h>
#endif

namespace TWlib {


/**
 * A two-way semaphore class. Will signal when counter is above zero, or when it decrements.
 * Good for queue type work.
 */
class TW_SemaTwoWay {
protected:
	int cnt;
	int size;

// Yes, two cond variables can share the same mutex:
//	http://stackoverflow.com/questions/4062126/can-2-pthread-condition-variables-share-the-same-mutex
	pthread_mutex_t localMutex; // thread safety for FIFO
	pthread_cond_t gtZeroCond;    // signaled when the count is larger than zero
	pthread_cond_t decrementCond; // signaled when the count goes down
public:
	TW_SemaTwoWay(int init_count) :
	cnt( init_count ), size( init_count )
	{
		pthread_mutex_init( &localMutex, NULL );
		pthread_cond_init( &gtZeroCond, NULL );
		pthread_cond_init( &decrementCond, NULL );
	}


	void reset() {
		pthread_mutex_lock( &localMutex );
		cnt = size;
		pthread_mutex_unlock( &localMutex );
	}

	void resetNoLock() {
		cnt = size;
	}

	/**
	 * Acuire the semaphore, waiting indefinitely. Waits until the semaphore is positive, then decrements the semaphore by
	 * one.
	 * @return
	 */
	int acquire() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
		while(cnt < 1) {
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA wait (acquire) [%p]\n",this);
#endif
			ret = pthread_cond_wait( &gtZeroCond, &localMutex ); // wait for change in cnt
			if(ret) {
#ifdef _TW_SEMA_HEAVY_DEBUG
				printf ("TW_SEMA acquire error (errno %d) (cnt=%d) [%p]\n",ret, cnt, this);
#endif
				break;
			}
		}
		if(!ret) {
			cnt--;
			pthread_cond_signal( &decrementCond );
		}
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	int acquire(const struct timespec *abstime) {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
		while(cnt < 1) {
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA wait (acquire) [%p]\n",this);
#endif
			ret = pthread_cond_timedwait( &gtZeroCond, &localMutex, abstime ); // wait for change in cnt
			if(ret) {
#ifdef _TW_SEMA_HEAVY_DEBUG
				printf ("TW_SEMA acquire error (errno %d) (cnt=%d) [%p]\n",ret, cnt, this);
#endif
				break;
			}
		}
		if(!ret) {
			cnt--;
			pthread_cond_signal( &decrementCond );
		}
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	int acquireAndKeepLock() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
		while(1) {
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA acquire wait (cnt=%d) [%p]\n",cnt, this);
#endif
			if(cnt >= 1) break;
			ret = pthread_cond_wait( &gtZeroCond, &localMutex ); // wait for change in cnt
			if(ret) {
#ifdef _TW_SEMA_HEAVY_DEBUG
				printf ("TW_SEMA acquire error (errno %d) (cnt=%d) [%p]\n",ret, cnt, this);
#endif
				break;
			}
		}
		if(!ret) {
			cnt--;
			pthread_cond_signal( &decrementCond );
		}
		return ret;
	}

	bool acquireAndKeepLockNoBlock() {
		bool ret = false;
		pthread_mutex_lock( &localMutex );
		if(cnt >= 1) {
			cnt--;
			pthread_cond_signal( &decrementCond );
			ret = true;
		}
		return ret;
	}

	int acquireAndKeepLock(const struct timespec *abstime) {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
		while(1) {
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA acquire wait (cnt=%d) [%p]\n",cnt, this);
#endif
			if(cnt >= 1) break;
			ret = pthread_cond_timedwait( &gtZeroCond, &localMutex, abstime ); // wait for change in cnt
			if(ret) {
#ifdef _TW_SEMA_HEAVY_DEBUG
				printf ("TW_SEMA acquire error or timeout (errno %d) (cnt=%d) [%p]\n",ret, cnt, this);
#endif
				break;
			}
		}
		if(!ret) {
			cnt--;
			pthread_cond_signal( &decrementCond );
		}
		return ret;
	}

	/**
	 * An easier to use acquire waiting for a given number of microseconds.
	 * @param usec_wait
	 * @return
	 */
	int acquire(const int64_t usec_wait ) {
		timeval tv;
		timespec ts;
		gettimeofday(&tv, NULL);
		TWlib::add_usec_to_timeval(usec_wait, &tv);
		TWlib::timeval_to_timespec(&tv,&ts);
		return acquire( &ts );
	}

	int acquireAndKeepLock(const int64_t usec_wait ) {
		timeval tv;
		timespec ts;
		gettimeofday(&tv, NULL);
		TWlib::add_usec_to_timeval(usec_wait, &tv);
		TWlib::timeval_to_timespec(&tv,&ts);
		return acquireAndKeepLock( &ts );
	}

	void lockSemaOnly() {
		pthread_mutex_lock( &localMutex );
	}


	void releaseSemaLock() {
		pthread_mutex_unlock( &localMutex );
	}

	int waitForAcquirers(bool lock = true) {
		int ret = 0;
		if(lock)
			pthread_mutex_lock( &localMutex );
		while(1) {
			if(cnt >= size)
				ret = pthread_cond_wait( &decrementCond, &localMutex );
		}
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	int waitForAcquirers(const struct timespec *abstime, bool lock = true) {
		int ret = 0;
		if(lock)
			pthread_mutex_lock( &localMutex );
		while(1) {
			if(cnt >= size)
				ret = pthread_cond_timedwait( &decrementCond, &localMutex, abstime );
		}
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	int waitForAcquirers(const int64_t usec_wait, bool lock = true ) {
		timeval tv;
		timespec ts;
		gettimeofday(&tv, NULL);
		TWlib::add_usec_to_timeval(usec_wait, &tv);
		TWlib::timeval_to_timespec(&tv,&ts);
		return waitForAcquirers( &ts, lock );
	}


	int waitForAcquirersKeepLock(bool lock = true) {
		int ret = 0;
		if(lock)
			pthread_mutex_lock( &localMutex );
		while(1) {
			if(cnt < size) break;
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA waitForAcquirers decrement (cnt=%d) [%p]\n",cnt, this);
			if(cnt > size) printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEK!!!\n");
#endif
			ret = pthread_cond_wait( &decrementCond, &localMutex );
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA got decrement (cnt=%d)\n",cnt);
#endif
		}
		return ret;
	}


	int waitForAcquirersKeepLock(const struct timespec *abstime, bool lock = true) {
		int ret = 0;
		if(lock)
			pthread_mutex_lock( &localMutex );
		while(1) {
			if(cnt < size) break;
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA waitForAcquirers decrement (cnt=%d) [%p]\n",cnt, this);
			if(cnt > size) printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEK!!!\n");
#endif
			ret = pthread_cond_timedwait( &decrementCond, &localMutex, abstime );
#ifdef _TW_SEMA_HEAVY_DEBUG
			if(!ret) printf ("TW_SEMA got decrement (cnt=%d)\n",cnt);
			else printf("TW_SEMA got error or timeout (cnt=%d)\n",cnt);
#endif
		}
		return ret;
	}


	int waitForAcquirersKeepLock(const int64_t usec_wait, bool lock = true ) {
		timeval tv;
		timespec ts;
		gettimeofday(&tv, NULL);
		TWlib::add_usec_to_timeval(usec_wait, &tv);
		TWlib::timeval_to_timespec(&tv,&ts);
		return waitForAcquirersKeepLock( &ts, lock );
	}

	int waitForDecrementKeepLock(const struct timespec *abstime, bool lock = true) {
		if(lock) pthread_mutex_lock( &localMutex );
		return pthread_cond_timedwait( &decrementCond, &localMutex, abstime );
	}

	int waitForDecrementKeepLock( const int64_t usec_wait, bool lock = true ) {
		timeval tv;
		timespec ts;
		gettimeofday(&tv, NULL);
		TWlib::add_usec_to_timeval(usec_wait, &tv);
		TWlib::timeval_to_timespec(&tv,&ts);
		if(lock)
			pthread_mutex_lock( &localMutex );
		return pthread_cond_timedwait( &decrementCond, &localMutex, &ts );
	}


//	int waitForDecrementKeepLock(bool lock = true) {
//		int ret = 0;
//		if(lock) {
//			pthread_mutex_lock( &localMutex );
//		}
//		ret = pthread_cond_wait( &decrementCond, &localMutex );
//		return ret;
//	}
//
//	int waitForDecrementKeepLock(const struct timespec *abstime, bool lock = true) {
//		if(lock) pthread_mutex_lock( &localMutex );
//		return pthread_cond_timedwait( &decrementCond, &localMutex, abstime );
//	}
//
//	int waitForDecrementKeepLock( const int64_t usec_wait, bool lock = true ) {
//		timeval tv;
//		timespec ts;
//		gettimeofday(&tv, NULL);
//		TWlib::add_usec_to_timeval(usec_wait, &tv);
//		TWlib::timeval_to_timespec(&tv,&ts);
//		if(lock)
//			pthread_mutex_lock( &localMutex );
//		return pthread_cond_timedwait( &decrementCond, &localMutex, &ts );
//	}

	/**
	 * Acuire or wait until an absolute time. Waits until the semaphore is positive, then decrements the semaphore by
	 * one.
	 * @param abstime
	 * @return
	 */
//	int acquire(const struct timespec *abstime) {
//		int ret = 0;
//		pthread_mutex_lock( &localMutex );
//		if(cnt < 1) {
//#ifdef _TW_SEMA_HEAVY_DEBUG
//			printf ("TW_SEMA timedwait [%p]\n",this);
//#endif
//			ret = pthread_cond_timedwait( &gtZeroCond, &localMutex, abstime ); // wait for change in cnt
//			if(ret == 0) {// if we waited successfully, and no timeout
//				cnt--;   //   the decrement the count down one
//#ifdef _TW_SEMA_HEAVY_DEBUG
//				printf ("TW_SEMA decrementing [%p]\n",this);
//#endif
//			}
//		}
////		if(deleteOnZero && cnt < 1)
////			delete this;
////		else
//		pthread_mutex_unlock( &localMutex );
////		printf ("TW_SEMA acquire done\n");
//		return ret;
//	}
//
//	int acquireAndKeepLock(const struct timespec *abstime) {
//		int ret = 0;
//		pthread_mutex_lock( &localMutex );
//		if(cnt < 1) {
//#ifdef _TW_SEMA_HEAVY_DEBUG
//			printf ("TW_SEMA timedwait [%p]\n",this);
//#endif
//			ret = pthread_cond_timedwait( &gtZeroCond, &localMutex, abstime ); // wait for change in cnt
//			if(ret == 0) {// if we waited successfully, and no timeout
//				cnt--;   //   the decrement the count down one
//#ifdef _TW_SEMA_HEAVY_DEBUG
//				printf ("TW_SEMA decrementing [%p]\n",this);
//#endif
//			}
//		}
//		return ret;
//	}





	/**
	 * Increments the counter, and alerts anyone waiting.
	 * @return
	 */
	int release() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
#ifdef _TW_SEMA_HEAVY_DEBUG
		printf ("TW_SEMA (release) incrementing [%p]\n",this);
#endif
		cnt++;
		if(cnt > 0) { // the 'if' should not be necessary
			ret = pthread_cond_signal( &gtZeroCond );
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA signaled [%p]\n",this);
#endif
		}
		pthread_mutex_unlock( &localMutex );
		return ret;
	}


	int releaseWithoutLock() {
		int ret = 0;
		cnt++;
#ifdef _TW_SEMA_HEAVY_DEBUG
		printf ("TW_SEMA (release) incrementing (%d) [%p]\n",cnt, this);
#endif
		if(cnt > 0) { // the 'if' should not be necessary
			ret = pthread_cond_signal( &gtZeroCond );
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA signaled [%p]\n",this);
#endif
		}
		return ret;
	}
	/**
	 * Increments the counter, and alerts anyone waiting.
	 * @return
	 */
	int releaseAndKeepLock() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
#ifdef _TW_SEMA_HEAVY_DEBUG
		printf ("TW_SEMA (release) incrementing [%p]\n",this);
#endif
		cnt++;
		if(cnt > 0) // the 'if' should not be necessary
			ret = pthread_cond_signal( &gtZeroCond );
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA signaled [%p]\n",this);
#endif
		return ret;
	}


	/**
	 * Releases all blocking calls on the TW_Sema. It's *not safe* to use the TW_Sema after this
	 * as the value of the sema is uknown.
	 * @return
	 */
	int releaseAll() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
#ifdef _TW_SEMA_HEAVY_DEBUG
		printf ("TW_SEMA incrementing [%p]\n",this);
#endif
		cnt++;
//		if(cnt > 0) // the 'if' should not be necessary
		pthread_cond_broadcast( &decrementCond );
		ret = pthread_cond_broadcast( &gtZeroCond );
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA signaled [%p]\n",this);
#endif
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	/**
	 * returns the current count (value) on the semaphore
	 * @return
	 */
	int count() {
		int ret = 0;
		pthread_mutex_lock( &localMutex );
		ret = cnt;
		pthread_mutex_unlock( &localMutex );
		return ret;
	}

	int countNoBlock() {
		return cnt;
	}

	/**
	 * flags the TW_Sema to destroy itself when the semaphore's internal count reaches zero.
	 * To be used like:
	 * sema->release();
	 * sema->flagDeleteAtZero(); ---> deletes immediately if at zero, or waits until zero, then deletes self
	 * ..some other thread:
	 * sema-acquire();
	 *
	 * .. use with care
	 */
/*	BAD IDEA - prevents stuff from being declared on stack easily
 * void flagDeleteAtZero() {
		pthread_mutex_lock( &localMutex );  // yes, order is important here...
		deleteOnZero = true;
		if(cnt < 1)
			delete this;
		else
			pthread_mutex_unlock( &localMutex );
	}
*/

	~TW_SemaTwoWay() {
#ifdef _TW_SEMA_HEAVY_DEBUG
			printf ("TW_SEMA DESTRUCTOR [%p]\n",this);
#endif
//		if(!deleteOnZero) // if deleteOnZero is set, then this mutex is locked already
		pthread_mutex_lock( &localMutex );  // yes, order is important here...
		pthread_cond_broadcast( &gtZeroCond );
		pthread_cond_broadcast( &decrementCond );
		pthread_cond_destroy( &gtZeroCond );
		pthread_cond_destroy( &decrementCond );
		pthread_mutex_unlock( &localMutex );
		pthread_mutex_destroy( &localMutex );

	}
};

} // end namespace

#endif /* TW_SEMA_H_ */
