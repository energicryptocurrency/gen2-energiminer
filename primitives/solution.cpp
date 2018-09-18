/*
 * solution.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "solution.h"
#include "common/common.h"
#include "common/Log.h"

#include <sstream>

using namespace energi;

std::string Solution::getBlockTransaction() const
{
    if (!m_work.isValid()) {
        throw WorkException("Invalid work, solution must be wrong!");
    }
    return m_work.getBlockTransaction();
}

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

