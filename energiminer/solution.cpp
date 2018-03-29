/*
 * solution.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "solution.h"
#include "energiminer/common.h"
#include "energiminer/Log.h"

#include <sstream>

using namespace energi;

std::string Solution::getSubmitBlockData() const
{
    if (!m_work.isValid()) {
        throw WorkException("Invalid work, solution must be wrong!");
    }
    std::stringstream ss;
    //! TODO check and provid correct nType and nVersion for this operation
    m_work.Serialize(ss, (1 << 0), 70208);
    return strToHex(ss.str());
}

