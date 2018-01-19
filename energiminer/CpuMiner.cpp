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
  egihash::result_t CpuMiner::GetPOWHash(uint32_t height, uint32_t nonce, const void *input)
  {
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(input);
    egihash::h256_t headerHash(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
    egihash::result_t ret;
    // if we have a DAG loaded, use it
    auto const & dag = ActiveDAG();

    if (dag && ((height / egihash::constants::EPOCH_LENGTH) == dag->epoch()))
    {
      ret = egihash::full::hash(*dag, headerHash, nonce);
    }
    else
    {
      // otherwise all we can do is generate a light hash
      ret = egihash::light::hash(egihash::cache_t(height), headerHash, nonce);
    }

    auto hashMix = ret.mixhash;
    if (std::memcmp(hashMix.b, egihash::empty_h256.b, egihash::empty_h256.hash_size) == 0)
    {
        throw WorkException("Can not produce a valid mixhash");
    }

//    // return a Keccak-256 hash of the full block header, including nonce and mixhash
//    CBlockHeaderFullLE fullBlockHeader(input, nonce, hashMix.b);
//    egihash::h256_t blockHash(&fullBlockHeader, sizeof(fullBlockHeader));
    //return std::make_tuple(ret.mixhash, ret.value);
    return ret;
  }

  egihash::h256_t CpuMiner::GetHeaderHash(const void *input)
  {
    energi::CBlockHeaderTruncatedLE truncatedBlockHeader(input);
    return egihash::h256_t(&truncatedBlockHeader, sizeof(truncatedBlockHeader));
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
        //! TODO remove testnet60 this is hardcoded for debug reason
        namespace fs = boost::filesystem;
    #ifdef WIN32
        return fs::path(getenv("APPDATA") + std::string("/EnergiCore/energiminer"));
    #else
        fs::path result;
        char* homePath = getenv("HOME");
        if (homePath == nullptr || strlen(homePath) == 0) {
            result = fs::path("/");
        } else {
            result = fs::path(homePath);
        }
    #ifdef MAC_OSX
        return result / "Library/Application Support/EnergiCore/energiminer";
    #else
        return result /".energicore/energiminer";
    #endif
    #endif
    }


  void CpuMiner::InitDAG(egihash::progress_callback_type callback)
  {
    using namespace egihash;

    auto const & dag = ActiveDAG();
    if (!dag)
    {
      auto const height = 0;// TODO (max)(GetHeight(), 0);
      auto const epoch = height / constants::EPOCH_LENGTH;
      auto const & seedhash = cache_t::get_seedhash(0).to_hex();
      std::stringstream ss;
      ss << std::hex << std::setw(4) << std::setfill('0') << epoch << "-" << seedhash.substr(0, 12) << ".dag";
      auto const epoch_file = GetDataDir() / "dag" / ss.str();

      printf("\nDAG file for epoch %u is \"%s\"", epoch, epoch_file.string().c_str());
      // try to load the DAG from disk
      try
      {
        std::unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), callback));
        ActiveDAG(move(new_dag));
        printf("\nDAG file \"%s\" loaded successfully. \n\n\n", epoch_file.string().c_str());


        egihash::dag_t::data_type data = dag->data();
        for ( int i = 0; i < 16; ++i )
        {
          std::cout << "NODE " << std::dec << i << std::hex << " " << data[0][i].hword << std::endl;
        }

        return;
      }
      catch (hash_exception const & e)
      {
        printf("\nDAG file \"%s\" not loaded, will be generated instead. Message: %s\n", epoch_file.string().c_str(), e.what());
      }

      // try to generate the DAG
      try
      {
        std::unique_ptr<dag_t> new_dag(new dag_t(height, callback));
        boost::filesystem::create_directories(epoch_file.parent_path());
        new_dag->save(epoch_file.string());
        ActiveDAG(move(new_dag));
        printf("\nDAG generated successfully. Saved to \"%s\".\n", epoch_file.string().c_str());
      }
      catch (hash_exception const & e)
      {
        printf("\nDAG for epoch %u could not be generated: %s", epoch, e.what());
      }
    }
    printf("\nDAG has been initialized already. Use ActiveDAG() to swap.\n");

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

  CpuMiner::CpuMiner(const Plant &plant, int index)
    :Miner("cpu", plant, index)
  {
    // First time init egi hash dag
    // We got to do for every epoch change below
    InitEgiHashDag();
  }

  void CpuMiner::trun()
  {
    try
    {
      while (true)
      {
        Work work = this->work(); // This work is a copy of last assigned work the worker was provided by plant

        if ( !work.isValid() )
        {
          cnote << "No work received. Pause for 1 s.";
          std::this_thread::sleep_for(std::chrono::seconds(1));

          if ( this->shouldStop() )
          {
            break;
          }

          continue;
        }
        else
        {
          //cnote << "Valid work.";
        }


        const uint32_t first_nonce = nonceStart_.load();
        const uint32_t max_nonce = nonceEnd_.load();
        //uint64_t hashes_done;

        uint32_t _ALIGN(128) hash[8];
        uint32_t _ALIGN(128) endiandata[29];
        uint32_t *pdata = work.blockHeader.data();
        uint32_t *ptarget = work.targetBin.data();

        const uint32_t Htarg = ptarget[7];
        //const uint32_t first_nonce = pdata[20];
        uint32_t nonce = first_nonce;
        uint32_t last_nonce = first_nonce;

        for (int k=0; k < 20; k++) // we dont use mixHash part to calculate hash but fill it later (below)
        {
          be32enc(&endiandata[k], pdata[k]);
        }

        do
        {
          //be32enc(&endiandata[28], nonce);
          auto hash_res = GetPOWHash(work.height, nonce, endiandata);
          //auto hashBlock  = std::get<0>(hash_res);
          //auto hashPOW    = std::get<1>(hash_res);
          memcpy(hash, hash_res.value.b, sizeof(hash_res.value));
          uint32_t arr[8] = {0};
          memcpy(arr, hash_res.mixhash.b, sizeof(hash_res.mixhash));

          for (int i = 0; i < 8; i++)
          {
            pdata[i + 20] = be32dec(&arr[i]);
          }

          if (hash[7] <= Htarg && fulltest(hash, ptarget))
          {
            auto nonceForHash = be32dec(&nonce);
            pdata[28] = nonceForHash;
            //pdata[20] = nonceForHash;
            //hashes_done = nonce - first_nonce;

            //cnote << "HASH:" << GetHex(hash_res.value.b, 32);

            /*CBlockHeaderFullLE fullBlockHeader(endiandata, nonce, hash_res.mixhash.b);
            egihash::h256_t blockHash(&fullBlockHeader, sizeof(fullBlockHeader));*/

//            cnote << "HASH MIX:" << GetHex(hash_res.mixhash.b, 32);
//            cnote << "GET FULL HASH:" << GetHex(blockHash.b, 32);
//            //cnote << "BlockHeader:" << GetHex((uint8_t*)work.blockHeader.data(), work.blockHeader.size() * 4);
//            cnote << "device:" << index_ << "nonce: " << nonce;
            addHashCount(nonce + 1 - last_nonce);

            Solution solution(work);
            plant_.submit(solution);

            return;
          }

          nonce++;

          if ( nonce % 10000 == 0 ) // rough guess
          {
            addHashCount(nonce - last_nonce);
            last_nonce = nonce;
          }

        } while (nonce < max_nonce || !this->shouldStop() );

        pdata[28] = be32dec(&nonce);
        //hashes_done = nonce - first_nonce + 1;
        addHashCount(nonce - last_nonce);
      }
    }
    catch(WorkException &ex)
    {
      cnote << ex.what();
    }
    catch(...)
    {}
  }

} /* namespace energi */
