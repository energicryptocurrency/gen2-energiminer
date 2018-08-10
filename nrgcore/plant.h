/*
 * Plant.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef PLANT_H_
#define PLANT_H_

#include "primitives/solution.h"

namespace energi {

class Plant
{
public:
    virtual ~Plant() = default;

    virtual unsigned get_tstart() const = 0;
    virtual unsigned get_tstop() const = 0;

	/**
	 * @brief Called from a Miner to note a Work has a solution.
	 * @param _p The solution.
	 * @return true iff the solution was good (implying that mining should be .
	 */
    //virtual void submit(const Solution &m) const = 0;
    virtual void submitProof(const Solution &m) const = 0;
	virtual void failedSolution() = 0;
    virtual uint64_t get_nonce_scumbler() const = 0;
    virtual uint64_t get_start_nonce(const Work& work, unsigned idx) const = 0;
};

} //namespace energi

#endif /* PLANT_H_ */
