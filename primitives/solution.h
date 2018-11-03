/*
 * solution.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_SOLUTION_H_
#define ENERGIMINER_SOLUTION_H_

#include <functional>
#include <iostream>

#include "nrghash/nrghash.h"
#include "uint256.h"
#include "work.h"

namespace energi {

class Solution
{
public:
    Solution()
    {}

    Solution(const Work &work) :
        m_work(work)
    {}

    std::string getSubmitBlockData() const;
    std::string getBlockTransaction() const;

    inline const std::string& getJobName() const
    {
        return m_work.getJobName();
    }

    inline std::string getTime() const
    {
        std::stringstream stream;
        stream << std::setfill ('0') << std::setw(sizeof(m_work.nTime)*2)
               << std::hex << m_work.nTime;
        return stream.str();
    }

    inline std::string getExtraNonce2() const
    {
        return m_work.getExtraNonce2();
    }

    const Work& getWork() const
    {
        return m_work;
    }

    inline std::string getNonce() const
    {
        uint64_t nonce = m_work.getNonce();
        std::stringstream stream;
        stream << std::setfill ('0') << std::setw(sizeof(nonce)*2)
               << std::hex << nonce;
        return stream.str();
    }

    const uint256& getHashMix() const
    {
        return m_work.getHashMix();
    }

    const uint256& getMerkleRoot() const
    {
        return m_work.getMerkleRoot();
    }

    void reset()
    {
        m_work.reset();
    }

private:
    Work m_work;
};

using SolutionFoundCallback = std::function<void(const Solution&)>;

} /* namespace energi */

#endif /* ENERGIMINER_SOLUTION_H_ */
