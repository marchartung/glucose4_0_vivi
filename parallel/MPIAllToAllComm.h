/*
 * MPIAllToAllComm.h
 *
 *  Created on: 16.05.2019
 *      Author: hartung
 */

#ifndef PARALLEL_MPIALLTOALLCOMM_H_
#define PARALLEL_MPIALLTOALLCOMM_H_

#include <mpi/mpi.h>
#include <vector>
#include "core/SolverTypes.h"
#include "parallel/Timer.h"

namespace Glucose {


class SharedCompanion;

class MPI_AllToAllComm {
public:
	MPI_AllToAllComm(int & argc, char ** & argv, SharedCompanion * sc);
	~MPI_AllToAllComm();

	template<typename VecType>
	void addClause(const unsigned lbd, const VecType & c);
	void progress();
	void setFinished(const lbool res, const vec<lbool> & resvec);
	bool isFinished();

	int numRanks() const;
	int getRank() const;

	void setThreadId(const int id);

	void setNVars(const int numVars);

	void getModel(vec<lbool> & model);

private:
	bool hasOpenComm;
	lbool result;
	unsigned missedExportClauses;
	unsigned missedImportClauses;
	int thn;
	int ranks;
	int rankId;
	int buffer_size;
	int numVars;
	Timer commTimer;
	SharedCompanion & sc;
	ClauseAllocator ca;
	vec<Lit> tmpClause;
	vec<CRef> clauses;
	MPI_Request open_comm_req;
	int * mpi_recvbuffer;
	int * mpi_sendbuffer;
	lbool * mpi_resultbuffer;

	void finishComm(const bool hardWait = false);
	void startNewComm();
	void addNewClauses();

	void sendClauses(const int numLits);

};

inline void MPI_AllToAllComm::getModel(vec<lbool> & model)
{
	assert(rankId == 0);
	model.clear();
	model.growTo(numVars);
	for(int i=0;i<numVars;++i)
		model[i] = mpi_resultbuffer[i];
}

inline bool MPI_AllToAllComm::isFinished()
{
	return result != l_Undef;
}


inline int MPI_AllToAllComm::numRanks() const
{
	return ranks;
}
inline int MPI_AllToAllComm::getRank() const
{
	return rankId;
}

inline void MPI_AllToAllComm::setThreadId(const int id)
{
	thn = id;
}

template<typename VecType>
inline void MPI_AllToAllComm::addClause(const unsigned lbd, const VecType & c)
{
	clauses.push(ca.alloc(c));
	ca[clauses.last()].setLBD(lbd);
}

} /* namespace Concusat */

#endif /* PARALLEL_MPIALLTOALLCOMM_H_ */
