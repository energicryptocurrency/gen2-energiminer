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
#include <iostream>

namespace utility {

inline std::string toBase(unsigned number, int base)
{
    static const std::string bases = std::string("0123456789abcdef");
    std::string result = std::string();
    while (number > 0) {
        result = bases[number%base] + result;
        number /= base;
    }
    return result;
}

} //namespace utility

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

    inline std::string getTime() const
    {
        auto millisecondsSinceEpoch = std::chrono::system_clock::now().time_since_epoch();
        auto number = millisecondsSinceEpoch.count() / std::chrono::system_clock::period::den;
        return utility::toBase(number, 16);
    }

public:
    uint32_t m_nonce;

private:
    Work m_work;
};

using SolutionFoundCallback = std::function<void(const Solution&)>;

} /* namespace energi */

#endif /* ENERGIMINER_SOLUTION_H_ */
