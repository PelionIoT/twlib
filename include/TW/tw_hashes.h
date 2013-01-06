/*
 * tw_hashes.h
 *
 *  Created on: Nov 25, 2011
 *      Author: ed
 * (c) 2011, WigWag LLC
 */

#ifndef TW_HASHES_H_
#define TW_HASHES_H_

#include <limits.h>

namespace TWlib {

template<class T>
struct tw_hash {
	size_t operator()(T x) const {
		return 0;
	}
};

struct hashInt {
	inline size_t operator()(const int &x) const {
		return (size_t) (x);
	}
};



} // end namespace
#endif /* TW_HASHES_H_ */
