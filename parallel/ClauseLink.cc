/*
 * ClauseLink.cc
 *
 *  Created on: 16.04.2019
 *      Author: hartung
 */

#include "ClauseLink.h"
#include "core/SolverTypes.h"

namespace Glucose {

ClauseLink::~ClauseLink() {
	assert(numRefs.load() == 0);
}

bool ClauseLink::isVivified() const {
	States a = static_cast<States>(flag.load());
	return a == States::VIVIFIED || a == States::TRUE;
}

bool ClauseLink::canBeVivified() const {
	return flag.load() == static_cast<unsigned>(States::NONE);
}

bool ClauseLink::isTrue() const {
	return flag.load() == static_cast<unsigned>(States::TRUE);
}

bool ClauseLink::lockForVivi() {
	unsigned expected = static_cast<unsigned>(States::NONE);
	return flag.compare_exchange_strong(expected,
			static_cast<unsigned>(States::IN_PROGRESS));
}

void ClauseLink::setVivified(const vec<Lit> & in) {
	assert(flag.load() == static_cast<unsigned>(States::IN_PROGRESS));
	if (in.size() == 0)
		flag = static_cast<unsigned>(States::TRUE);
	else {
		in.copyTo(vivifiedClause);
		flag = static_cast<unsigned>(States::VIVIFIED);
	}
}


void ClauseLink::setFailed()
{
	assert(flag.load() == static_cast<unsigned>(States::IN_PROGRESS));

	flag = static_cast<unsigned>(States::FAILED);
}

const vec<Lit> & ClauseLink::getVivifiedClause() const {
	assert(flag.load() == static_cast<unsigned>(States::VIVIFIED));
	return vivifiedClause;
}

ClauseLink * ClauseLink::get(const unsigned numThreads) {
	return new ClauseLink(numThreads);
}

void ClauseLink::dereference() {
	unsigned refs = numRefs.fetch_sub(1);
	if (refs == 1)
		delete this;
}

ClauseLink::ClauseLink(const unsigned numThreads) :
		flag(), numRefs(numThreads) {
}

} /* namespace Glucose */
