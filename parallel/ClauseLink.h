/*
 * ClauseLink.h
 *
 *  Created on: 16.04.2019
 *      Author: hartung
 */

#ifndef PARALLEL_CLAUSELINK_H_
#define PARALLEL_CLAUSELINK_H_

#include "mtl/Vec.h"
#include <atomic>

namespace Glucose {

struct Lit;

class ClauseLink {
public:

	~ClauseLink();

	bool isVivified() const ;

	bool canBeVivified() const;

	bool isTrue() const ;

	bool lockForVivi();

	void setVivified(const vec<Lit> & in);

	void setFailed();

	unsigned getNumRefs() const
	{
		return numRefs.load();
	}

	const vec<Lit> & getVivifiedClause() const;
	static ClauseLink * get(const unsigned numThreads);

	void dereference();

private:
	std::atomic<unsigned> flag;
	std::atomic<unsigned> numRefs;
	vec<Lit> vivifiedClause;

	enum class States {
		NONE, VIVIFIED, TRUE, FAILED, IN_PROGRESS
	};

	ClauseLink(const unsigned numThreads) ;

};

} /* namespace Glucose */

#endif /* PARALLEL_CLAUSELINK_H_ */
