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

    Solution(Work work, uint64_t nonce, unsigned extraNonce, const uint256& mixhash)
        : m_nonce(nonce)
        , m_extraNonce(extraNonce)
        , m_mixhash(mixhash)
        , m_work(work)
    {}

    std::string getSubmitBlockData() const;

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

    inline std::string getExtraNonce() const
    {
        std::stringstream stream;
        stream << std::setfill ('0') << std::setw(sizeof(m_extraNonce)*2)
               << std::hex << m_extraNonce;
        return stream.str();
    }

public:
    uint64_t m_nonce;
    unsigned m_extraNonce;
    uint256 m_mixhash;

private:
    Work m_work;
};

using SolutionFoundCallback = std::function<void(const Solution&)>;

} /* namespace energi */

#endif /* ENERGIMINER_SOLUTION_H_ */
