/*
 * Work.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include <memory>
#include "primitives/base58.h"
#include "primitives/block.h"
#include "work.h"

namespace energi
{
  Work::Work(const Json::Value &gbt, const std::string &coinbase_addr, const std::string& job)
      : Block(gbt, coinbase_addr)
      , m_jobName(job)
  {
      hashTarget = arith_uint256().SetCompact(this->nBits);
  }
} /* namespace energi */
