/*
 * minercommon.h
 *
 *  Created on: Nov 22, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_MINERCOMMON_H_
#define ENERGIMINER_MINERCOMMON_H_


#include <jsoncpp/json/json.h>
#include "egihash/egihash.h"


namespace energi
{
    class miner_exception : public std::exception
    {
    public:
        miner_exception(const std::string &reason):m_reason(reason){}
        const char* what() const _GLIBCXX_USE_NOEXCEPT override
        {
            return m_reason.c_str();
        }
    private:
        std::string m_reason;
    };


    // Binary string of uint8_t, just like string is of type char
    using bstring8 = std::vector<uint8_t>;
    template <typename T> void set(const uint8_t* ptr, T value) { *reinterpret_cast<T*>(const_cast<uint8_t*>(ptr)) = value; }

    // 32 byte vector
    using bstring32 = std::vector<uint32_t>;

	struct Work
	{
    	// Valid work
        Work(const Json::Value &value, const std::string &coinbase_addr) throw(miner_exception);
        // No work, probably initialized with assignment
        Work(){}
        Work(const Work&) = default;
        Work(Work &&) = delete;
        Work& operator = (const Work &) = default;
        Work& operator = (Work &&) = delete;

        bool operator==(const Work& another)
        {
            return this->target == another.target && this->height == another.height;
        }

        int             			height = 0;
        bstring32           		target;
        bstring32           		data;
        bstring8            		coinbase_txn;
        std::string         		m_txn_data;

        int data_bytes_size() const
        {
            return data.size() * sizeof(uint32_t);
        }
	};

	// Solution basically is used to submit block
	// Solution is initialized with the work once miner mines a block
	struct Solution
	{
        bstring32           		data;
        std::string     transaction_hex;

        Solution(){}
        std::string getSubmitBlockData() const;
//        Solution& operator = (const Solution&)
//        {
//        }
	};

	// test
	class Worker
	{
		enum class State : unsigned char
		{
			Starting,
			Started,
			Stopping,
			Stopped,
			Killing
		};

	public:
		Worker(const std::string& name)
		: m_name(name)
		{}

		Worker(Worker const&) = default;
		Worker& operator=(Worker const&) = default;
		virtual ~Worker();

		/// Starts a new thread;
		bool start();
		/// Stop thread;
		bool stop();

		bool shouldStop() const
		{
			return m_state != State::Started;
		}

		// Since work is assigned to the worked by the plant and worker is doing the real
		// work in a thread, protect the data carefully.
		virtual void setWork(const Work& work)
		{
			std::lock_guard<std::mutex> lock(m_mutex_work);
			m_work = work;

			onSetWork();
		}

		// Work should be copied with a guard
		void copyWork(Work &dest) const
		{
			std::lock_guard<std::mutex> l(m_mutex_work);
			dest = m_work;
		}

	protected:
		virtual void onSetWork() = 0;
		// run in a thread
		// This function is meant to run some logic in a loop.
		// Should quit once done or when new work is assigned
		virtual void trun(){}
	private:

		std::string 					m_name;
		mutable std::mutex 				m_mutex_work; // protext work since its actually used in a thread
		Work							m_work;

		std::unique_ptr<std::thread> 	m_tworker;
		//std::atomic<State> 			m_state { State::Starting };
        State 				            m_state { State::Starting };
	};


    class MinePlant;
	class Miner: public Worker
	{
	public:
		Miner(std::string const& name, MinePlant &plant):
			Worker(name + std::to_string(0)),
            m_plant(plant)
		{}

		Miner(Miner && m) = default;

		virtual ~Miner() = default;

		void setWork(Work const& work)
		{
			Worker::setWork(work);
		}

	protected:
        MinePlant &m_plant;
	};


	class MinePlant
	{
	public:
		using CBSolutionFound = std::function<bool(const Solution&)>;
		using VMiners = std::vector<std::shared_ptr<Miner>>;

		~MinePlant()
		{
			stop();
		}

		bool start(const VMiners &vMiner
					, const CBSolutionFound& cb_solution_found)
		{
			//std::lock_guard<std::mutex> lock(m_mutex_miner);

			m_cb_solution_found = cb_solution_found;

			if ( m_started )
            {
                return m_started;
            }

			m_started = true;

            for ( auto &miner : vMiner)
            {
                m_miners.push_back(miner);
                m_started &= m_miners.back()->start();
            }

            // Failure ? return right away
            return m_started;
		}

		void stop()
		{
//			std::lock_guard<std::mutex> lock(m_mutex_miner);
			for ( auto &miner : m_miners)
			{
				miner->stop();
			}
		}

		void setWork(const Work& work)
		{
			//Collect hashrate before miner reset their work
			//collectHashRate();

			// Set work to each miner
			//std::lock_guard<std::mutex> lock(m_mutex_miner);
/*
			if (_wp.header == m_work.header && _wp.startNonce == m_work.startNonce)
			{
				return;
			}
*/

			m_work = work;
			for (auto &miner: m_miners)
			{
				miner->setWork(m_work);
			}
		}

		void restart()
		{
			stop();
		}


		void submit(const Solution &sol)
		{
			m_cb_solution_found(sol);
		}

		bool m_started = false;
        Solution m_solution;
        CBSolutionFound m_cb_solution_found;
	private:
        VMiners m_miners;
		Work m_work;

		//std::atomic<bool> m_isMining = {false};

		//mutable std::mutex x_progress;
		//mutable WorkingProgress m_progress;

		//SolutionFound m_onSolutionFound;
		//MinerRestart m_onMinerRestart;
        //bool m_has_found_block = false;
	};


    class CPUMiner: public Miner
    {
    public:
        CPUMiner(MinePlant& _farm);
        ~CPUMiner()
        {}
    protected:
        void onSetWork() override;
        void trun() override;
    };

    class CLMiner: public Miner
    {
    public:
        CLMiner(MinePlant& _farm);
        ~CLMiner();
    protected:
        void onSetWork() {};
        void trun();
    };
}

#endif /* ENERGIMINER_MINERCOMMON_H_ */
