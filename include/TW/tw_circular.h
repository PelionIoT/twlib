
// tupperware container lib
#ifndef _TW_CIRCULAR
#define _TW_CIRCULAR

// FIFO: a simple class to handle a fifo list of void pointers.
#include <pthread.h>
#include <errno.h>

//#include <TW/tw_log.h>

// #ifdef 'g++' whatever that is...
#include <new>
// #endif

#include <TW/tw_utils.h> // TimeVal
#include <TW/tw_sema2.h>
#include <TW/tw_alloc.h>
//#include "logging.h"

#if __cplusplus >= 201103L
#define TWLIB_HAS_MOVE_SEMANTICS 1
#endif
#ifdef TWLIB_HAS_MOVE_SEMANTICS
#include <utility>
#endif


//#define DEBUG_TW_CIRCULAR_H

#ifdef DEBUG_TW_CIRCULAR_H
#pragma message "!!!!!!!!!!!! tw_circular is Debug Heavy!!"
// confused? here: https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
#define TW_CIRCULAR_DBG_OUT(s,...) fprintf(stderr, "**DEBUG** " s "\n", ##__VA_ARGS__ )
#define IF_CIRCULAR_DBG_OUT( x ) { x }
#else
#define TW_CIRCULAR_DBG_OUT(s,...) {}
#define IF_CIRCULAR_DBG_OUT( x ) {}
#endif


namespace TWlib {





/**
 * A circular FIFO. This is a thread-safe, non and/or blocking queue using pthread conditions.
 * T must support:
 * self assignment: operator= (T &a, T &b) or move()
 * default constructor
 */
template <class T, class ALLOC>
class tw_safeCircular {
public:

//	class iter {
//	public:
//		iter() : n(0) { }
//		bool getNext(T &fill);
//		bool atEnd();
//		friend class tw_safeCircular;
//	protected:
//		int n;
//	};

	tw_safeCircular( int size, bool initobj = false );
	tw_safeCircular() = delete;
	tw_safeCircular( tw_safeCircular<T,ALLOC> &o ) = delete;
#ifdef _TW_WINDOWS
	tw_safeCircular( HANDLE theHeap );
#endif
	void add( T &d );
	bool add( T &the_d, const int64_t usec_wait  );
	bool addIfRoom( T &the_d );
#ifdef TWLIB_HAS_MOVE_SEMANTICS
	void addMv( T &d );
	bool addMv( T &the_d, const int64_t usec_wait  );
	bool addMvIfRoom( T &the_d );
#endif

	// array like functions
	bool get(int n, T &d );
	bool set(int n, T &d );
	bool setMv(int n, T &d );


	// unimplemented-->
	void transferFrom( tw_safeCircular<T,ALLOC> &other ); // transfer record from other to 'this' - can block
	bool transferFromNoBlock( tw_safeCircular<T,ALLOC> &other ); // transfer record from other to 'this' -
	                                               // wont block - false if would have blocked
	// <--unimplemented

	bool remove( T &fill ); // true if got data
#ifdef TWLIB_HAS_MOVE_SEMANTICS
	bool removeMv( T &fill );
#endif
	bool removeOrBlock( T &fill ); // true if removed something
//	bool removeOrBlock( T &fill, TimeVal &t );
	bool removeOrBlock( T &fill, const int64_t usec_wait );
	bool removeMvOrBlock( T &fill );
	bool removeMvOrBlock( T &fill, const int64_t usec_wait );
	void clearAll(); // remove all nodes (does not delete T)
//	void unblock();  // unblock 1 blocking call
	void unblockAll(); // unblock all blocking calls
	void disable();
	void enable();

	class iter final {
		friend class tw_safeCircular<T,ALLOC>;
	public:
		bool atEnd();
		bool data(T &);
		bool next();
		void release();
	protected:
		iter(tw_safeCircular &container);
		tw_safeCircular &owner;
		bool valid;
		int n; // count
		int p; // pointer into array
	};

	tw_safeCircular<T,ALLOC>::iter getIter();

//	void startIter( iter &i );
////	tw_safeCircular &operator=( const tw_safeCircular &o );
//
//	void releaseIter( iter &i );
	int remaining();
	~tw_safeCircular();
protected:
	bool isObjects;
	bool _reverse;
	bool full;
	TW_SemaTwoWay *sema;
	bool enabled; // if enabled the FIFO can take new values
	pthread_mutex_t newDataMutex; // thread safety for FIFO
	pthread_cond_t newdataCond;
	int _block_cnt;
	int nextNextIn() {
		int n;
		if(full) nextOut = nextNextOut();
		if(_reverse) {
			n = nextIn - 1;
			if(n < 0) n = _size - 1;
			if(n == nextOut) full = true;
		} else {
			n = nextIn + 1;
			if(n >= _size) n = 0;
			if(n == nextOut) full = true;
		}
		return n;
	}
	int nextNextOut() {
		int n;
		full = false;
		if(_reverse) {
			n = nextOut - 1;
			if(n < 0) n = _size - 1;
		} else {
			n = nextOut + 1;
			if(n >= _size) n = 0;
		}
		return n;
	}
	int remain() {  // nextIn is always ahead of nextOut. circular
		if(full) return _size;
		if(nextIn < 0) return 0;
		if(_reverse) {
			if(nextOut >= nextIn) return nextOut-nextIn;
			else return _size - (nextIn - nextOut);
		} else {
			if(nextOut <= nextIn) return nextIn-nextOut;
			else return _size - (nextOut - nextIn);
		}
	}
	int nextIn;   // position to place next in value
	int nextOut;  // position to pull next out value
	int _size;     // size of the Circular buffer - only set once.
	T *data;
//	ALLOC *alloc;
#ifdef _TW_WINDOWS
	HANDLE hHeap;
#endif

public:
	// flips the circular around, so that the first element is now the last
	void reverse() {
//		if(!full) {
			if(!_reverse) {
				int t = nextOut;
				nextOut = nextIn + 1;
				if(nextOut >= _size) nextOut = nextOut - _size;
				nextIn = t + 1;
				if(nextIn >= _size) nextIn = nextIn - _size;
			} else {
				int t = nextOut;
				nextOut = nextIn - 1;
				if(nextOut < 0) nextOut = _size - 1;
				nextIn = t - 1;
				if(nextIn < 0) nextIn = _size - 1;
			}
//		}
		_reverse = !(_reverse);
	}

};


#ifdef TWLIB_HAS_MOVE_SEMANTICS
#endif


/**
 * Unlimited sized FIFO. This is a thread-safe, blocking queue using pthread conditions.
 * T must support:
 * self assignment: operator= (T &&a, T &&b)  rvalue assignment required (regular assignment *not* required)
 * default constructor
 */


/** T must support:
 * self assignment: operator= (T &a, T &b)
 * default constructor
 */
/**
 * This is like the tw_safeCircular class, expect there is a bounded limit on the number of units it can hold.
 */
//template <class T,class ALLOC>
//class tw_bndSafeFIFOStatic {
//protected:
//	tw_safeCircular<T,ALLOC> _fifo; // we use the above FIFO class do the most fifo work here...
//public:
//
//	typedef typename tw_safeCircular<T,ALLOC>::iter iter;
//
//	tw_bndSafeFIFOStatic( void );
//	tw_bndSafeFIFOStatic( int maxsize );
//	tw_bndSafeFIFOStatic( int maxsize, ALLOC *_a );
//	tw_bndSafeFIFOStatic( int masize, tw_safeCircular<T,ALLOC> &o );
//	tw_bndSafeFIFOStatic( tw_bndSafeFIFOStatic<T,ALLOC> &o );
////#ifdef _TW_WINDOWSs
////	tw_safeCircular( HANDLE theHeap );
////#endif
//	void add( T &d );
//	void addToHead( T &d );
//	void transferFrom( tw_safeCircular<T,ALLOC> &other ); // transfer record from other to 'this' - can block
//	bool transferFromNoBlock( tw_safeCircular<T,ALLOC> &other ); // transfer record from other to 'this' -
//	                                               // wont block - false if would have blocked
//	T *addEmpty();
//	bool peek( T &fill ); // true if got valid value - look at next, dont remove
//	bool peekOrBlock( T &fill ); // true if got data - look at next, dont remove
//	bool peekOrBlock( T &fill, TimeVal &t );
//	bool remove( T &fill ); // true if got data
//	bool removeOrBlock( T &fill ); // true if removed something
//	bool removeOrBlock( T &fill, TimeVal &t );
//	void clearAll(); // remove all nodes (does not delete T)
//	void unblockRemoveCalls();  // unblock 1 blocking call
//	void unblockAll(); // unblock all blocking calls
//	void disable();
//	void enable();
//
//	void startIter( iter &i );
//	tw_bndSafeFIFOStatic<T,ALLOC> &operator=( const tw_bndSafeFIFOStatic<T,ALLOC> &o );
//
////	void removeAtIter( iter &i );
//	void releaseIter( iter &i );
//	int remaining();
//	~tw_bndSafeFIFOStatic();
//protected:
//	TW_Sema *_sizeSema; // use this semaphore to not over fill the FIFO
//	int _max;
//};


}

using namespace TWlib;

/////////////////////////////////////////////////////////////////////////
// thread safe FIFO


#ifdef _TW_WINDOWS
template <class T,class ALLOC>
tw_safeCircular<T,ALLOC>::tw_safeCircular( HANDLE theHeap ) : enabled( true ) {
	alloc = NULL;
	out = (tw_FIFO_link *) NULL;
	in = (tw_FIFO_link *) NULL;
	remain = 0;
	hHeap = theHeap;
}
#endif

template <class T,class ALLOC>
tw_safeCircular<T,ALLOC>::tw_safeCircular( int size, bool initobj ) : isObjects(initobj), _reverse(false), full( false ), enabled( true ),
	_block_cnt(0), nextIn(-1), nextOut(-1), _size(size), data(NULL) {
//	alloc = NULL;
	pthread_mutex_init( &newDataMutex, NULL );
	pthread_cond_init( &newdataCond, NULL );
	data = (T *) ALLOC::malloc( size * sizeof(T) );
	sema = new TW_SemaTwoWay(size);
	if(isObjects) {
		for(int n=0;n<size;n++) {
			T *p = data + n;
			p = new (p) T(); // placement new, if objects require an init.
		}
	}
}


//template<class T, class ALLOC>
//tw_safeCircular<T,ALLOC>::tw_safeCircular(tw_safeCircular<T,ALLOC> &o) : enabled( true ) {
//	alloc = NULL;
//	_block_cnt = 0;
//	pthread_mutex_init(&dataMutex, NULL);
//	pthread_cond_init(&newdataCond, NULL);
//	out = (tw_FIFO_link *) NULL;
//	in = (tw_FIFO_link *) NULL;
//	remain = 0;
//
//	*this = o;
//}

//template <class T,class ALLOC>
//tw_safeCircular<T,ALLOC>::tw_safeCircular( ALLOC *a ) : enabled( true ) {
//	alloc = a;
//	_block_cnt = 0;
////	dataMutex = PTHREAD_MUTEX_INITIALIZER;
////	newdataCond = PTHREAD_COND_INITIALIZER;
//	pthread_mutex_init( &dataMutex, NULL );
//	pthread_cond_init( &newdataCond, NULL );
//	out = (tw_FIFO_link *) NULL;
//	in = (tw_FIFO_link *) NULL;
//	remain = 0;
//}

//template <class T,class ALLOC>
//inline void tw_safeCircular<T,ALLOC>::startIter( iter &i ) {
//	i.look = out;
//	pthread_mutex_lock(&newDataMutex);
//}
//
//template <class T,class ALLOC>
//inline void tw_safeCircular<T,ALLOC>::releaseIter( iter &i ) {
//	i.look = NULL;
//	pthread_mutex_unlock(&newDataMutex);
//}



// copies other tw_safeCircular - does not copy Allocator
//template <class T,class ALLOC>
//tw_safeCircular<T,ALLOC> &tw_safeCircular<T,ALLOC>::operator=( const tw_safeCircular<T,ALLOC> &o ) {
//	tw_FIFO_link *look;
//	tw_FIFO_link *prev;
//	tw_FIFO_link *newlink;
//
//	this->clearAll(); // clear anything that might be there
//
//	pthread_mutex_lock(const_cast<pthread_mutex_t *>(&(o.dataMutex)));
//	pthread_mutex_lock(&dataMutex);
//	this->enabled = o.enabled;
//	this->remain = 0;
//	/*
//#ifdef _TW_WINDOWS
//	newlink = (tw_FIFO_link *) HeapAlloc( hHeap, 0, sizeof( tw_FIFO_link ));
//#else
//	if(alloc)
//		newlink = (tw_FIFO_link *) alloc->malloc( sizeof( tw_FIFO_link ));
//	else
//		newlink = (tw_FIFO_link *) ACE_OS::malloc( sizeof( tw_FIFO_link ));
//#endif
//	newlink->init_link(the_d);
//*/
//	look = o.out;
//
//	if(look) {
//	newlink = (tw_FIFO_link *) ALLOC::malloc( sizeof( tw_FIFO_link ));
//	newlink->init_link(look->d);
//	prev = newlink;
//	this->out = newlink;
//	look = look->next;
//	this->remain++;
//	}
//
//	while(look) {
//		newlink = (tw_FIFO_link *) ALLOC::malloc( sizeof( tw_FIFO_link ));
//		newlink->init_link(look->d);
//		prev->next = newlink; // link to link behind us
//		prev = newlink;       // move forward
//		look = look->next;    // move the source forward
//		this->remain++;
//	}
//
//	this->in = prev;
//	pthread_mutex_unlock(const_cast<pthread_mutex_t *>(&(o.dataMutex)));
//	pthread_mutex_unlock(&dataMutex);
//	return *this;
//}

/**
 * enables the FIFO, allowing the adding of new items.
 * Items already in the FIFO can be pulled out regardless.
 */
template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::enable() {
	sema->lockSemaOnly();
	enabled = true;
	sema->releaseSemaLock();
}

/**
 * disables the FIFO, preventing the adding of new items.
 * Items already in the FIFO can be pulled out.
 */
template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::disable() {
	sema->lockSemaOnly();
	enabled = false;
	sema->releaseSemaLock();
}


/**
 * Will block if queue is full
 */
template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::add( T &the_d ) {
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add()");
	sema->acquireAndKeepLock();
	nextIn = nextNextIn();
	data[nextIn] = the_d;
	TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
	sema->releaseSemaLock();
//	unblock(); // let one blocking call know...
}

#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::addMv( T &the_d ) {
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add(move)");
	sema->acquireAndKeepLock();
	nextIn = nextNextIn();
	data[nextIn] = std::move(the_d);
	TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
	sema->releaseSemaLock();
//	unblock(); // let one blocking call know...
}
#endif

// will block is queue is full!!
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::addIfRoom( T &the_d ) {
	bool ret = false;
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add()");
	if(sema->acquireAndKeepLockNoBlock()) {
		nextIn = nextNextIn();
		data[nextIn] = the_d;
		TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
		ret = true;
	} else {
		TW_CIRCULAR_DBG_OUT("not adding. no room: %d",remain());
	}
	sema->releaseSemaLock();
	return ret;
}

#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::addMvIfRoom( T &the_d ) {
	bool ret = false;
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add(move)");
	if(sema->acquireAndKeepLockNoBlock()) {
		nextIn = nextNextIn();
		data[nextIn] = std::move(the_d);
		TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
		ret = true;
	} else {
		TW_CIRCULAR_DBG_OUT("not adding. no room: %d",remain());
	}
	sema->releaseSemaLock();
	return ret;
}
#endif


// will block is queue is full!!
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::add( T &the_d, const int64_t usec_wait  ) {
	bool ret = true;
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add()");
	int r = sema->acquireAndKeepLock(usec_wait);
	if(!r) {
		nextIn = nextNextIn();
		data[nextIn] = the_d;
		TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
	} else {
		TW_CIRCULAR_DBG_OUT("timeout / error on circular buffer: remain = %d",remain());
		ret = false;
	}
	sema->releaseSemaLock();
	return ret;
}

#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::addMv( T &the_d, const int64_t usec_wait  ) {
	bool ret = true;
	TW_CIRCULAR_DBG_OUT("acquireAndKeepLock - add(move)");
	int r = sema->acquireAndKeepLock(usec_wait);
	if(!r) {
		nextIn = nextNextIn();
		data[nextIn] = std::move(the_d);
		TW_CIRCULAR_DBG_OUT("remain post-add(): %d",remain());
	} else {
		TW_CIRCULAR_DBG_OUT("timeout / error on circular buffer: remain = %d",remain());
		ret = false;
	}
	sema->releaseSemaLock();
	return ret;
}
#endif

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::get(int n, T &d ) {
	bool ret = false;
	sema->lockSemaOnly();
	if((n >= 0) && (n < remain())) {
		int c = 0;
		int p = nextOut;
		if(_reverse) {
			p = nextIn -1;
			if(p < 0) p = _size -1;
		}
		p++;
		while(c != n) {
			if(p >= _size) p = 0;
			p++; c++;
		}
		d = data[p];
		ret = true;
	}
	sema->releaseSemaLock();
	return ret;
}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::set(int n, T &d ) {
	bool ret = false;
	sema->lockSemaOnly();
	if((n >= 0) && (n < remain())) {
		int c = 0;
		int p = nextOut;
		if(_reverse) {
			p = nextIn -1;
			if(p < 0) p = _size -1;
		}
		p++;
		while(c != n) {
			if(p >= _size) p = 0;
			p++; c++;
		}
		data[p] = d;
		ret = true;
	}
	sema->releaseSemaLock();
	return ret;
}

#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::setMv(int n, T &d ) {
	bool ret = false;
	sema->lockSemaOnly();
	if((n >= 0) && (n < remain())) {
		int c = 0;
		int p = nextOut;
		if(_reverse) {
			p = nextIn -1;
			if(p < 0) p = _size -1;
		}
		p++;
		while(c != n) {
			if(p >= _size) p = 0;
			p++; c++;
		}
		data[p] = std::move(d);
		ret = true;
	}
	sema->releaseSemaLock();
	return ret;
}
#endif

/*
class iter {
	bool atEnd();
	bool data(T &);
	bool next();
	void release();
protected:
	iter(tw_safeCircular &container);
	tw_safeCircular &owner
	bool valid;
	int n; // count
	int p; // pointer into array
};
*/

template <class T,class ALLOC>
typename tw_safeCircular<T,ALLOC>::iter tw_safeCircular<T,ALLOC>::getIter() {
	return tw_safeCircular<T,ALLOC>::iter(*this);
}

template <class T,class ALLOC>
tw_safeCircular<T,ALLOC>::iter::iter(tw_safeCircular<T,ALLOC> &c) :
	owner(c), valid(false), n(0), p(0)
{
	c.sema->lockSemaOnly();
	p = c.nextOut;
	p++;
	if(p >= c._size) p = 0;
	if(c.remain() > 0)
		valid = true;
}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::iter::next() {
	n++;
	if(n < owner.remain()) {
		p++;
		if(p >= owner._size) p = 0;
		return true;
	} else {
		valid = false;
		return false;
	}
}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::iter::data(T &d) {
	if(valid && n < owner.remain()) {
		d = owner.data[p];
		return true;
	} else
		return false;
}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::iter::atEnd() {
	if(valid && n < owner.remain()) {
		return false;
	} else
		return true;
}

template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::iter::release() {
	owner.sema->releaseSemaLock();
	valid = false;
}


template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::remove( T &fill ) {
	bool ret = true;
	sema->lockSemaOnly();
	if(remain() > 0) {
		sema->releaseWithoutLock();
		nextOut = nextNextOut();
		fill = data[nextOut];
	} else {
		ret = false;
	}
	sema->releaseSemaLock();
	return ret;
}

#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::removeMv( T &fill ) {
	bool ret = true;
	sema->lockSemaOnly();
	if(remain() > 0) {
		sema->releaseWithoutLock();
		nextOut = nextNextOut();
		fill = std::move(data[nextOut]);
	} else {
		ret = false;
	}
	sema->releaseSemaLock();
	return ret;
}
#endif

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::removeOrBlock( T &fill ) {
	bool ret = true;
	sema->lockSemaOnly();
	TW_CIRCULAR_DBG_OUT("removeOrBlock.. remain = %d", remain());
	if(remain() > 0) {
		sema->releaseWithoutLock();
		TW_CIRCULAR_DBG_OUT("   ...removeOrBlock(2).. remain = %d", remain());
		nextOut = nextNextOut();
		fill = data[nextOut];
		sema->releaseSemaLock();
	} else {
		TW_CIRCULAR_DBG_OUT("  ...removeOrBlock(%d) waitForAcquirers", remain());
		int r = sema->waitForAcquirersKeepLock(false); // unlocks while waiting for acquire
		if(!r) {
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers complete. remain = %d", remain());
			sema->releaseWithoutLock();
			nextOut = nextNextOut();
			fill = data[nextOut];
		} else {
			ret = false;
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers error (%d). remain = %d", r, remain());
		}
		sema->releaseSemaLock();
	}
	return ret;
}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::removeOrBlock( T &fill, const int64_t usec_wait ) {
	bool ret = true;
	sema->lockSemaOnly();
	TW_CIRCULAR_DBG_OUT("removeOrBlock.. remain = %d", remain());
	if(remain() > 0) {
		sema->releaseWithoutLock();
		TW_CIRCULAR_DBG_OUT("   ...removeOrBlock(2).. remain = %d", remain());
		nextOut = nextNextOut();
		fill = data[nextOut];
		sema->releaseSemaLock();
	} else {
		TW_CIRCULAR_DBG_OUT("  ...removeOrBlock(%d) waitForAcquirers", remain());
		int r = sema->waitForAcquirersKeepLock(usec_wait, false); // unlocks while waiting for acquire
		if(!r) {
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers complete. remain = %d", remain());
			sema->releaseWithoutLock();
			nextOut = nextNextOut();
			fill = data[nextOut];
		} else {
			ret = false;
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers timeout or error (%d). remain = %d", r, remain());
		}
		sema->releaseSemaLock();
	}
	return ret;
}


#ifdef TWLIB_HAS_MOVE_SEMANTICS
template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::removeMvOrBlock( T &fill ) {
	bool ret = true;
	sema->lockSemaOnly();
	TW_CIRCULAR_DBG_OUT("removeOrBlock.. remain = %d", remain());
	if(remain() > 0) {
		sema->releaseWithoutLock();
		TW_CIRCULAR_DBG_OUT("   ...removeOrBlock(2).. remain = %d", remain());
		nextOut = nextNextOut();
		fill = std::move(data[nextOut]);
		sema->releaseSemaLock();
	} else {
		TW_CIRCULAR_DBG_OUT("  ...removeOrBlock(%d) waitForAcquirers", remain());
		int r = sema->waitForAcquirersKeepLock(false); // unlocks while waiting for acquire
		if(!r) {
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers complete. remain = %d", remain());
			sema->releaseWithoutLock();
			nextOut = nextNextOut();
			fill = std::move(data[nextOut]);
		} else {
			ret = false;
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers error (%d). remain = %d", r, remain());
		}
		sema->releaseSemaLock();
	}
	return ret;

}

template <class T,class ALLOC>
bool tw_safeCircular<T,ALLOC>::removeMvOrBlock( T &fill, const int64_t usec_wait ) {
	bool ret = true;
	sema->lockSemaOnly();
	TW_CIRCULAR_DBG_OUT("removeOrBlock.. remain = %d", remain());
	if(remain() > 0) {
		sema->releaseWithoutLock();
		TW_CIRCULAR_DBG_OUT("   ...removeOrBlock(2).. remain = %d", remain());
		nextOut = nextNextOut();
		fill = std::move(data[nextOut]);
		sema->releaseSemaLock();
	} else {
		TW_CIRCULAR_DBG_OUT("  ...removeOrBlock(%d) waitForAcquirers", remain());
		int r = sema->waitForAcquirersKeepLock(usec_wait, false); // unlocks while waiting for acquire
		if(!r) {
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers complete. remain = %d", remain());
			sema->releaseWithoutLock();
			nextOut = nextNextOut();
			fill = std::move(data[nextOut]);
		} else {
			ret = false;
			TW_CIRCULAR_DBG_OUT("  ...waitForAcquirers timeout or error (%d). remain = %d", r, remain());
		}
		sema->releaseSemaLock();
	}
	return ret;

}

#endif



//
//template <class T,class ALLOC>
//bool tw_safeCircular<T,ALLOC>::peek( T &fill ) {
//	bool ret = false;
//	//	if(in==out)
//	//	in = NULL;
//	pthread_mutex_lock(&dataMutex);
//	if(out) {
//		ret = true;
//		fill = out->d;
//	}
//	pthread_mutex_unlock(&newDataMutex);
//	return ret;
//}
//
//template <class T,class ALLOC>
//bool tw_safeCircular<T,ALLOC>::peekOrBlock( T &fill ) {
//	bool ret = false;
//	//	if(in==out)
//	//	in = NULL;
//	pthread_mutex_lock(&newDataMutex);
//	if(!out) {
//		_block_cnt++;
//		pthread_cond_wait( &newdataCond, &newDataMutex ); // wait until new data arrives
//	}
//	if(out) {
//		ret = true;
//		fill = out->d;
//	}
//	pthread_mutex_unlock(&newDataMutex);
//	return ret;
//}
//
//template <class T,class ALLOC>
//bool tw_safeCircular<T,ALLOC>::peekOrBlock( T &fill, TimeVal &t ) {
//	bool ret = false;
//	//	if(in==out)
//	//	in = NULL;
//	pthread_mutex_lock(&newDataMutex);
//	if(!out) {
//		_block_cnt++;
//		int err = pthread_cond_timedwait( &newdataCond, &newDataMutex, t.timespec() ); // wait until new data arrives
//		if(err == ETIMEDOUT) { // if timeout, don't look at value.
//#ifdef _TW_CIRCULAR_DEBUG_ON
//		TW_DEBUG_LT("pthread_cond_timedwait - peek ETIMEDOUT\n",NULL);
//#endif
//			pthread_mutex_unlock(&newDataMutex);
//			return false;
//		}
//	}
//	if(out) {
//		ret = true;
//		fill = out->d;
//	}
//	pthread_mutex_unlock(&newDataMutex);
//	return ret;
//}

template <class T,class ALLOC>
int tw_safeCircular<T,ALLOC>::remaining(void) {
	int ret;
	sema->lockSemaOnly();
	ret = remain();
	sema->releaseSemaLock();
	return ret;
}

template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::clearAll() { // delete all remaining links (and hope someone took care of the data in each of those)

}

template <class T,class ALLOC>
void tw_safeCircular<T,ALLOC>::unblockAll() {
	sema->releaseAll();
}



template <class T,class ALLOC>
tw_safeCircular<T,ALLOC>::~tw_safeCircular() { // delete all remaining links (and hope someone took care of the data in each of those)
	unblockAll();
	pthread_mutex_lock(&newDataMutex);
	if(isObjects) { // cleanup objects if needed
		int end = nextOut;
		do {
			data[nextOut].~T();
			nextOut++;
			if(nextOut > _size) nextOut = 0;
		} while(nextOut != end);
	}
	pthread_cond_destroy(&newdataCond);   // NEW
	pthread_mutex_unlock(&newDataMutex);
	pthread_mutex_destroy(&newDataMutex);    // NEW
	// Should call pthread_cond_destroy
}



#endif // _TW_FIFO
