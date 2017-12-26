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

namespace energi
{
  std::string Solution::getSubmitBlockData() const
  {
    if ( !work_.isValid() )
    {
      throw WorkException("Invalid work, solution must be wrong!");
    }

    std::string blockHeaderStr(2 * work_.blockHeader.size() * sizeof(uint32_t) + 1, 0);
    for( auto &v : work_.blockHeader)
    {
      be32enc(const_cast<uint32_t*>(&v), v);
    }

    bin2hex(const_cast<char*>(blockHeaderStr.c_str()), (unsigned char *)work_.blockHeader.data(), 116);
    std::stringstream ss;
    ss << blockHeaderStr.c_str() << work_.rawTransactionData;



    cdebug << "JOIN: " << ss.str();


    cnote << work_.ToString();

    return ss.str();
  }

} /* namespace energi */
