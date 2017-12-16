/*
 * CpuMiner.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "CpuMiner.h"
#include "energiminer/Log.h"
#include "energiminer/common.h"
#include <iomanip>
#include <mutex>
#include <iostream>
#include <sstream>


namespace energi
{
  egihash::h256_t CpuMiner::egihash_calc(uint32_t height, uint32_t nonce, const void *input)
  {
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(input);
    egihash::h256_t headerHash(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
    egihash::result_t ret;
    // if we have a DAG loaded, use it
    auto const & dag = ActiveDAG();

    if (dag && ((height / egihash::constants::EPOCH_LENGTH) == dag->epoch()))
    {
      return egihash::full::hash(*dag, headerHash, nonce).value;
    }

      // otherwise all we can do is generate a light hash
      // TODO: pre-load caches and seed hashes
      return egihash::light::hash(egihash::cache_t(height, egihash::get_seedhash(height)), headerHash, nonce).value;
  }



  std::unique_ptr<egihash::dag_t> const & CpuMiner::ActiveDAG(std::unique_ptr<egihash::dag_t> next_dag)
  {
      using namespace std;

      static std::mutex m;
      MutexLGuard lock(m);
      static std::unique_ptr<egihash::dag_t> active; // only keep one DAG in memory at once

      // if we have a next_dag swap it
      if (next_dag)
      {
          active.swap(next_dag);
      }

      // unload the previous dag
      if (next_dag)
      {
          next_dag->unload();
          next_dag.reset();
      }

      return active;
  }


  boost::filesystem::path CpuMiner::GetDataDir()
  {
    namespace fs = boost::filesystem;
    return fs::path("/home/ranjeet/.energicore/regtest/");
  }


  void CpuMiner::InitDAG(egihash::progress_callback_type callback)
  {
    using namespace egihash;

    auto const & dag = ActiveDAG();
    if (!dag)
    {
      auto const height = 0;// TODO (max)(GetHeight(), 0);
      auto const epoch = height / constants::EPOCH_LENGTH;
      auto const & seedhash = seedhash_to_filename(get_seedhash(height));
      std::stringstream ss;
      ss << std::hex << std::setw(4) << std::setfill('0') << epoch << "-" << seedhash.substr(0, 12) << ".dag";
      auto const epoch_file = GetDataDir() / "dag" / ss.str();

      printf("DAG file for epoch %u is \"%s\"", epoch, epoch_file.string().c_str());
      // try to load the DAG from disk
      try
      {
        std::unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), callback));
        ActiveDAG(move(new_dag));
        printf("DAG file \"%s\" loaded successfully. \n\n\n", epoch_file.string().c_str());


        egihash::dag_t::data_type data = dag->data();
        for ( int i = 0; i < 16; ++i )
        {
          std::cout << "NODE " << std::dec << i << std::hex << " " << data[0][i].hword << std::endl;
        }

        return;
      }
      catch (hash_exception const & e)
      {
        printf("DAG file \"%s\" not loaded, will be generated instead. Message: %s", epoch_file.string().c_str(), e.what());
      }

      // try to generate the DAG
      try
      {
        std::unique_ptr<dag_t> new_dag(new dag_t(height, callback));
        boost::filesystem::create_directories(epoch_file.parent_path());
        new_dag->save(epoch_file.string());
        ActiveDAG(move(new_dag));
        printf("DAG generated successfully. Saved to \"%s\".", epoch_file.string().c_str());
      }
      catch (hash_exception const & e)
      {
        printf("DAG for epoch %u could not be generated: %s", epoch, e.what());
      }
    }
    printf("DAG has been initialized already. Use ActiveDAG() to swap.");

    /*egihash::dag_t::data_type data = dag->data();
    std::cout << "DAG 0: " << std::hex << data.size() << std::endl;*/
  }


  bool CpuMiner::InitEgiHashDag()
  {
    // initialize the DAG
    InitDAG([](::std::size_t step, ::std::size_t max, int phase) -> bool
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2)
        << static_cast<double>(step) / static_cast<double>(max) * 100.0 << "%"
        << std::setfill(' ') << std::setw(80);

        auto progress_handler = [&](std::string const &msg)
        {
          std::cout << "\r" << msg;
        };

      switch(phase)
      {
        case egihash::cache_seeding:
            progress_handler("Seeding cache ... ");
          break;
        case egihash::cache_generation:
            progress_handler("Generating cache ... ");
          break;
        case egihash::cache_saving:
            progress_handler("Saving cache ... ");
          break;
        case egihash::cache_loading:
            progress_handler("Loading cache ... ");
          break;
        case egihash::dag_generation:
            progress_handler("Generating Dag ... ");
          break;
        case egihash::dag_saving:
            progress_handler("Saving Dag ... ");
          break;
        case egihash::dag_loading:
            progress_handler("Loading Dag ... ");
          break;
        default:
          break;
      }

      auto progress = ss.str();
      std::cout << progress << std::flush;

      return true;
    });

    return true;
  }

  void CpuMiner::trun()
  {
    Work current_work;
    try
    {
      while (true)
      {
        Work work = this->work(); // This work is a copy of last assigned work the worker was provided by plant

        if ( !work.isValid() )
        {
          cdebug << "Invalid work received. Pause for 3 s.";
          std::this_thread::sleep_for(std::chrono::seconds(3));
          continue;
        }

        const uint32_t first_nonce = nonceStart_.load();
        const uint32_t max_nonce = nonceEnd_.load();
        //uint64_t hashes_done;

        uint32_t _ALIGN(128) hash[8];
        uint32_t _ALIGN(128) endiandata[21];
        uint32_t *pdata = work.blockHeader.data();
        uint32_t *ptarget = work.targetBin.data();

        const uint32_t Htarg = ptarget[7];
        //const uint32_t first_nonce = pdata[20];
        uint32_t nonce = first_nonce;
        uint32_t last_nonce = first_nonce;

        for (int k=0; k < 20; k++)
        {
          be32enc(&endiandata[k], pdata[k]);
        }

        do
        {
          be32enc(&endiandata[20], nonce);
          auto hash_res = egihash_calc(work.height, nonce, endiandata);
          memcpy(hash, hash_res.b, sizeof(hash_res.b));

          if (hash[7] <= Htarg && fulltest(hash, ptarget))
          {
            auto nonceForHash = be32dec(&nonce);
            pdata[20] = nonceForHash;
            //hashes_done = nonce - first_nonce;

            addHashCount(nonce - last_nonce);

            Solution solution(current_work);
            plant_.submit(solution);

            return;
          }

          nonce++;

          if ( nonce % 10000 == 0 ) // rough guess
          {
            addHashCount(nonce - last_nonce);
            last_nonce = nonce;
          }

        } while (nonce < max_nonce || !isStopped() );

        pdata[20] = be32dec(&nonce);
        //hashes_done = nonce - first_nonce + 1;
        addHashCount(nonce - last_nonce);
      }
    }
    catch(...)
    {

    }
  }

} /* namespace energi */
