/*
 * CpuMiner.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_CPUMINER_H_
#define ENERGIMINER_CPUMINER_H_

#include "energiminer/miner.h"
#include "energiminer/egihash/egihash.h"

#include <memory>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace energi
{
  class CpuMiner : public Miner
  {
  public:
    CpuMiner(const Plant &plant, int index)
      :Miner("cpu", plant, index)
    {
    }

    static std::unique_ptr<egihash::dag_t> const & ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag  = std::unique_ptr<egihash::dag_t>());
    static boost::filesystem::path GetDataDir();
    static void InitDAG(egihash::progress_callback_type callback);
    static bool InitEgiHashDag();

    static egihash::h256_t egihash_calc(uint32_t height, uint32_t nonce, const void *input);


    virtual ~CpuMiner()
    {}

  protected:
    void trun();
  };

} /* namespace energi */

#endif /* ENERGIMINER_CPUMINER_H_ */
