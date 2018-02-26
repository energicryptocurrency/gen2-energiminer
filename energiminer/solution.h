/*
 * solution.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_SOLUTION_H_
#define ENERGIMINER_SOLUTION_H_

#include "energiminer/work.h"

#include <functional>

namespace energi {

class Solution
{
public:
    Solution()
    {}

    Solution(Work work, uint32_t nonce)
        : m_nonce(nonce)
        , m_work(work)
    {}

    std::string getSubmitBlockData() const;

    inline const std::string& getJobName() const
    {
        return m_work.getJobName();
    }

public:
    uint32_t m_nonce;

private:
    Work m_work;
};

using SolutionFoundCallback = std::function<void(const Solution&)>;

} /* namespace energi */

#endif /* ENERGIMINER_SOLUTION_H_ */
