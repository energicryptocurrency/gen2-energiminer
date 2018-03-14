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

    std::string blockHeaderStr(2 * m_work.blockHeader.size() * sizeof(uint32_t) + 1, 0);
    for( auto &v : m_work.blockHeader) {
        be32enc(const_cast<uint32_t*>(&v), v);
    }

    bin2hex(const_cast<char*>(&blockHeaderStr[0]), (unsigned char *)m_work.blockHeader.data(), 116);
    std::stringstream ss;
    ss << blockHeaderStr.c_str() << m_work.rawTransactionData;

    cdebug << "JOIN: " << ss.str();
    cnote << m_work.ToString();
    return ss.str();
}

