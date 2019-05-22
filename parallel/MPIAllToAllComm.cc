/*
 * MPIAllToAllComm.cc
 *
 *  Created on: 16.05.2019
 *      Author: hartung
 */

#include "SharedCompanion.h"
#include "MPIAllToAllComm.h"
#include "utils/Options.h"
#include "mtl/XAlloc.h"
#include "mtl/Sort.h"

#include <ctime>
#include <iostream>
#include <limits>

namespace Glucose {
char mpi_tag[] = "MPI";
DoubleOption opt_mpi_interval(mpi_tag, "mpi-int",
		"MPI communication interval. 0 disables MPI", 2.0,
		DoubleRange(0, false, HUGE_VAL, true));
IntOption opt_mpibuffersizes(mpi_tag, "mpi-buffer",
		"Number of MB used for MPI send and receive", 5);

MPI_AllToAllComm::MPI_AllToAllComm(int & argc, char ** & argv,
		SharedCompanion* sc) :
		hasOpenComm(false), result(l_Undef), missedExportClauses(0), missedImportClauses(
				0), thn(-1), ranks(1), rankId(0), buffer_size(0), numVars(0), commTimer(
				opt_mpi_interval), sc(*sc), open_comm_req(), mpi_recvbuffer(
				nullptr), mpi_sendbuffer(nullptr), mpi_resultbuffer(nullptr) {
	static_assert(sizeof(lbool) == sizeof(uint8_t),"Logical type has mismatch with mpi impl");
	if (opt_mpi_interval > 0.0) {
		MPI_Init(&argc, &argv);
		MPI_Comm_rank(MPI_COMM_WORLD, &rankId);
		MPI_Comm_size(MPI_COMM_WORLD, &ranks);
		if (ranks > 1) {
			buffer_size = ((long int) opt_mpibuffersizes * 1024 * 1024)
					/ sizeof(int) / ranks;
			MPI_Alloc_mem(ranks * buffer_size * sizeof(int), MPI_INFO_NULL,
					&mpi_recvbuffer);
			MPI_Alloc_mem(buffer_size * sizeof(int), MPI_INFO_NULL,
					&mpi_sendbuffer);
		}
	}
}

MPI_AllToAllComm::~MPI_AllToAllComm() {
	if (opt_mpi_interval > 0.0) {

		int flag;
		MPI_Finalized(&flag);
		if (!flag) {
			if (ranks > 1) {
				MPI_Barrier(MPI_COMM_WORLD); // after barrier, everybody should have an outstanding all to all
				assert(!hasOpenComm);

				MPI_Free_mem(mpi_recvbuffer);
				MPI_Free_mem(mpi_sendbuffer);
				MPI_Free_mem(mpi_resultbuffer);
				mpi_recvbuffer = nullptr;
				mpi_sendbuffer = nullptr;
				mpi_resultbuffer = nullptr;
			}
			MPI_Finalize();
		} else
			assert(mpi_recvbuffer == nullptr);

	}
}

void MPI_AllToAllComm::progress() {
	if (ranks > 1) {
		addNewClauses();
		if (hasOpenComm)
			finishComm();

		if (!hasOpenComm && !isFinished() && commTimer.isOver())
			startNewComm();
	}
}

void MPI_AllToAllComm::setFinished(const lbool res, const vec<lbool> & resvec) {

	if (ranks > 1) {
		if (hasOpenComm)
			finishComm(true);
		if (!isFinished()) {
			result = res;
			mpi_sendbuffer[0] = std::numeric_limits<int>::max();
			mpi_sendbuffer[1] = rankId;
			mpi_sendbuffer[2] = toInt(res);
			sendClauses(3);
			for (int i = 0; i < resvec.size(); ++i)
				mpi_resultbuffer[i] = resvec[i];
			finishComm(true);
			MPI_Barrier(MPI_COMM_WORLD);
		}
	} else
		result = res;
}

void MPI_AllToAllComm::setNVars(const int numVars) {
	this->numVars = numVars;
	MPI_Alloc_mem(sizeof(lbool) * numVars, MPI_INFO_NULL, &mpi_resultbuffer);
}

void MPI_AllToAllComm::finishComm(const bool hardWait) {
	int flag = 0, finishingRank = -1;
	assert(hasOpenComm);
	if (hardWait) {
		MPI_Wait(&open_comm_req, MPI_STATUS_IGNORE);
		flag = 1;
	} else {
		MPI_Test(&open_comm_req, &flag, MPI_STATUS_IGNORE);
	}
	if (flag) {
		unsigned numImportedCl = 0;
		int clauseSize;
		int * cur;
		hasOpenComm = false;
		for (int i = 0; i < ranks; ++i) {
			if (i != rankId) {
				cur = &mpi_recvbuffer[i * buffer_size];
				clauseSize = *cur;
				while (clauseSize > 0
						&& clauseSize != std::numeric_limits<int>::max()) {
					tmpClause.clear();
					tmpClause.growTo(clauseSize);
					for (int k = 0; k < clauseSize; ++k) {
						tmpClause[k].x = *(++cur);
						assert(std::abs(*cur) < 2 * numVars);
					}
					if (tmpClause.size() > 1) {
						if (!sc.addLearnt(thn, tmpClause))
							++missedImportClauses;
					}
					else
						sc.addLearnt(thn,tmpClause[0]);
					clauseSize = *(++cur);
					++numImportedCl;
				}
				if (clauseSize == std::numeric_limits<int>::max()) {
					std::cout << "haha\n";
					finishingRank = *(++cur);
					result = lbool(static_cast<uint8_t>(*(++cur)));
					assert(result == l_True || result == l_False);
					sc.someOneFinished();
					break;
				}
			}
		}
		commTimer.reset();
		if (rankId == 0)
			std::cout << "c rank 0: imported " << numImportedCl
					<< " clauses missed imports " << missedImportClauses
					<< " missed exports " << missedExportClauses << "\n";
		if (isFinished()) {
			if (rankId == 0 && finishingRank != -1)
				std::cout << "c rank " << finishingRank << "is on fire\n";

			if (rankId != 0
					&& (finishingRank == -1 || finishingRank > rankId)) {
				MPI_Send(mpi_resultbuffer, numVars, MPI_UINT8_T, 0, 0,
				MPI_COMM_WORLD);
			} else if (rankId == 0 && finishingRank > 1)
				MPI_Recv(mpi_resultbuffer, numVars, MPI_UINT8_T, finishingRank,
						0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}

	}

}

struct ClauseComp {
	const ClauseAllocator & ca;
	ClauseComp(const ClauseAllocator & ca) :
			ca(ca) {
	}

	bool operator()(const CRef r1, const CRef r2) const {
		const Clause & c1 = ca[r1], &c2 = ca[r2];
		if (c1.lbd() == c2.lbd())
			return c1.size() < c2.size();
		else
			return c1.lbd() < c2.lbd();
	}
};
void MPI_AllToAllComm::sendClauses(const int numLits) {
	assert(!hasOpenComm);
	assert(!isFinished() || numLits == 3);
	int n = buffer_size;
	MPI_Ialltoall(mpi_sendbuffer, n, MPI_INT, mpi_recvbuffer, n, MPI_INT,
	MPI_COMM_WORLD, &open_comm_req);
	hasOpenComm = true;

}

void MPI_AllToAllComm::startNewComm() {
	assert(ranks > 1);
	assert(!hasOpenComm);
	sort(clauses, ClauseComp(ca));
	int numLits = 0, i = 0;
	for (; i < clauses.size(); ++i) {
		const Clause & c = ca[clauses[i]];
		if (numLits + c.size() + 2 < buffer_size) {
			mpi_sendbuffer[numLits++] = c.size();
			for (int k = 0; k < c.size(); ++k)
				mpi_sendbuffer[numLits + k] = toInt(c[k]);
			numLits += c.size();
		} else
			break;
	}
	missedExportClauses += clauses.size() - i;
	mpi_sendbuffer[numLits++] = 0;
	clauses.clear(false);
	ca.reset();
	sendClauses(numLits);

}

void MPI_AllToAllComm::addNewClauses() {
	assert(ranks > 1);
	int importedFromThread;
	unsigned lbd, num = 0;
	while (sc.getNewClause(thn, importedFromThread, tmpClause, lbd)) {
		addClause(lbd, tmpClause);
		++num;
	}
	Lit l;
	tmpClause.clear();
	tmpClause.growTo(1);
	while ((tmpClause[0] = sc.getUnary(thn)) != lit_Undef) {
		addClause(0, tmpClause);
		++num;
	}
//	if (num > 0)
//		std::cout << rankId << ": added " << num << " clauses to database\n";
}

} /* namespace Concusat */
