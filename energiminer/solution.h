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

    Solution(const Work &work)
        : work_(work)
    {}

    std::string getSubmitBlockData() const;

private:
    Work work_;
};

using SolutionFoundCallback = std::function<void(const Solution&)>;

} /* namespace energi */

#endif /* ENERGIMINER_SOLUTION_H_ */
