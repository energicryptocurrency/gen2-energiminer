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

    enum class WorkProgress
    {
        Started,
        Done
    };

    class miner_exception : public std::exception
    {
    public:
        miner_exception(const std::string &reason)
        :m_reason(reason){
        }

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
        Work(const Json::Value &value, const std::string &coinbase_addr) throw(miner_exception);
        Work()
        {
            // no work, probably initialized with assignment
        }

        bool operator==(const Work& another)
        {
            return this->target == another.target && this->data == another.data;
        }

        int             			height;
        bstring32           		target;
        bstring32           		data;
        bstring8            		coinbase_txn;
        std::string         		m_txn_data;

        int data_bytes_size() const
        {
            return data.size() * sizeof(uint32_t);
        }
	};

	struct Solution
	{
		uint64_t        nonce;
		egihash::h256_t hash;
        std::string     data;
        Solution()
        {}

        Solution(const Work &work);
	};

	// test
	class Worker
	{
		enum class State
		{
			Starting,
			Started,
			Stopping,
			Stopped,
			Killing
		};

	public:
		Worker(std::string const& name)
		: m_name(name)
		{
		}

		Worker(Worker const&) {};
		Worker& operator=(Worker const&) {};
		virtual ~Worker();

		/// Starts a new thread;
		bool start();
		/// Stop thread;
		bool stop();

		bool shouldStop() const
		{
			return m_state != State::Started;
		}

		virtual void setWork(Work const& work)
		{
			std::lock_guard<std::mutex> lock(m_mwork);
			m_work = work;

			pause();
			kickOff();
		}

		Work work() const
		{
			std::lock_guard<std::mutex> l(m_mwork);
			return m_work;
		}

	protected:
		virtual void kickOff(){};
		virtual void pause() {};
	private:
		virtual void workLoop(){};

		std::string 					m_name;
		mutable std::mutex 				m_mwork;
		Work							m_work;

		std::unique_ptr<std::thread> 	m_tworker;
		//std::atomic<State> 				m_state { State::Starting };
        State 				            m_state { State::Starting };
	};


    class Plant;
	class Miner: public Worker
	{
	public:
		Miner(std::string const& name, Plant &plant):
			Worker(name + std::to_string(0)),
			m_index(0),
            m_plant(plant)
		{}

		virtual ~Miner() = default;

		void setWork(Work const& work)
		{
			Worker::setWork(work);
			//m_hashCount = 0;
		}

		uint64_t hashCount() const { return m_hashCount; }
		void resetHashCount() { m_hashCount = 0; }

	protected:

		void addHashCount(uint64_t _n) { m_hashCount += _n; }

		/*static unsigned s_dagLoadMode;
		static volatile unsigned s_dagLoadIndex;
		static unsigned s_dagCreateDevice;
		static volatile void* s_dagInHostMemory;*/

		const size_t m_index = 0;
        Plant &m_plant;
	private:
		uint64_t m_hashCount = 0;
		//mutable Mutex x_work;
	};



	class PlantFace
	{
	public:
		virtual ~PlantFace() = default;

		/**
		 * @brief Called from a Miner to note a WorkPackage has a solution.
		 * @param _p The solution.
		 * @return true iff the solution was good (implying that mining should be .
		 */
		//virtual bool submitProof(Solution const& _p) = 0;
	};

	class Plant : public PlantFace
	{
	public:
		~Plant()
		{
			stop();
		}

		/**
		 * @brief Start a number of miners.
		 */
		bool start(std::vector<Miner> &vMiner)
		{
			std::lock_guard<std::mutex> l(x_minerWork);
            if ( m_started )
            {
                return true;
            }

            m_started = true;
            for ( auto &miner : vMiner)
            {
                m_started &= miner.start();
            }

            if ( !m_started )
            {
                return false;
            }
            m_started = true;
/*
			// Already started
			if (!m_miners.empty())
			{
				return true;
			}


			unsigned start = 0;
			m_miners.reserve(1);

			// Assume we have one device for one miner
			for (unsigned i = start; i < 1; ++i)
			{
				// TODO: Improve miners creation, use unique_ptr.
				m_miners.push_back(std::shared_ptr<Miner>(new CPUMiner(*this, 0)));
				// Start miners' threads. They should pause waiting for new work
				// package.
				m_miners.back()->start();
			}*/

			m_isMining = true;
			//b_lastMixed = mixed;

			/*if (!p_hashrateTimer)
			{
				p_hashrateTimer = new boost::asio::deadline_timer(m_io_service, boost::posix_time::milliseconds(1000));
				p_hashrateTimer->async_wait(boost::bind(&Plant::processHashRate, this, boost::asio::placeholders::error));
				if (m_serviceThread.joinable()) {
					m_io_service.reset();
				}
				else {
					m_serviceThread = std::thread{ boost::bind(&boost::asio::io_service::run, &m_io_service) };
				}
			}*/

			return true;
		}

		/**
		 * @brief Stop all mining activities.
		 */
		void stop()
		{
			std::lock_guard<std::mutex> l(x_minerWork);
			m_miners.clear();
			m_isMining = false;

/*
			if (p_hashrateTimer) {
				p_hashrateTimer->cancel();
			}
*/

/*
			m_io_service.stop();
			m_serviceThread.join();
*/
//				p_hashrateTimer = nullptr;
		}

		/**
		 * @brief Sets the current mining mission.
		 * @param _wp The work package we wish to be mining.
		 */
		void setWork(Work const& _wp)
		{
			//Collect hashrate before miner reset their work
			//collectHashRate();

			// Set work to each miner
			std::lock_guard<std::mutex> l(x_minerWork);
/*
			if (_wp.header == m_work.header && _wp.startNonce == m_work.startNonce)
			{
				return;
			}
*/

			m_work = _wp;
            m_hasFoundBlock = false;

			for (auto const& m: m_miners)
			{
				m->setWork(m_work);
			}
		}

		/*
		void collectHashRate()
		{
			WorkingProgress p;
			std::lock_guard<std::mutex> l2(x_minerWork);
			p.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastStart).count();
			//Collect
			for (auto const& i : m_miners)
			{
				uint64_t minerHashCount = i->hashCount();
				p.hashes += minerHashCount;
				p.minersHashes.push_back(minerHashCount);
			}

			//Reset
			for (auto const& i : m_miners)
			{
				i->resetHashCount();
			}
			m_lastStart = std::chrono::steady_clock::now();

			if (p.hashes > 0) {
				m_lastProgresses.push_back(p);
			}

			// We smooth the hashrate over the last x seconds
			int allMs = 0;
			for (auto const& cp : m_lastProgresses) {
				allMs += cp.ms;
			}
			if (allMs > m_hashrateSmoothInterval) {
				m_lastProgresses.erase(m_lastProgresses.begin());
			}
		}

		void processHashRate(const boost::system::error_code& ec) {

			if (!ec) {
				collectHashRate();
			}

			// Restart timer
			p_hashrateTimer->expires_at(p_hashrateTimer->expires_at() + boost::posix_time::milliseconds(1000));
			p_hashrateTimer->async_wait(boost::bind(&Plant::processHashRate, this, boost::asio::placeholders::error));
		}*/

		/**
		 * @brief Stop all mining activities and Starts them again
		 */
		void restart()
		{
			stop();
			//start(m_lastSealer, b_lastMixed);

			if (m_onMinerRestart)
			{
				m_onMinerRestart();
			}
		}

		bool isMining() const
		{
			return m_isMining;
		}

		/**
		 * @brief Get information on the progress of mining this work package.
		 * @return The progress with mining so far.

		WorkingProgress const& miningProgress() const
		{
			WorkingProgress p;
			p.ms = 0;
			p.hashes = 0;
			{
				std::lock_guard<std::mutex> l2(x_minerWork);
				for (auto const& i : m_miners) {
					(void) i; // unused
					p.minersHashes.push_back(0);
				}
			}

			for (auto const& cp : m_lastProgresses) {
				p.ms += cp.ms;
				p.hashes += cp.hashes;
				for (unsigned int i = 0; i < cp.minersHashes.size(); i++)
				{
					p.minersHashes.at(i) += cp.minersHashes.at(i);
				}
			}

			std::lock_guard<std::mutex> l(x_progress);
			m_progress = p;
			return m_progress;
		}*/

		/*SolutionStats getSolutionStats() {
			return m_solutionStats;
		}

		void failedSolution() {
			m_solutionStats.failed();
		}

		void acceptedSolution(bool _stale) {
			if (!_stale)
			{
				m_solutionStats.accepted();
			}
			else
			{
				m_solutionStats.acceptedStale();
			}
		}

		void rejectedSolution(bool _stale) {
			if (!_stale)
			{
				m_solutionStats.rejected();
			}
			else
			{
				m_solutionStats.rejectedStale();
			}
		}
*/
		using SolutionFound = std::function<bool(Solution const&)>;
		using MinerRestart = std::function<void()>;

		/**
		 * @brief Provides a valid header based upon that received previously with setWork().
		 * @param _bi The now-valid header.
		 * @return true if the header was good and that the Plant should pause until more work is submitted.
		 */
		//void onSolutionFound(SolutionFound const& _handler) { m_onSolutionFound = _handler; }
		//void onMinerRestart(MinerRestart const& _handler) { m_onMinerRestart = _handler; }

        bool m_started = false;
		Work work() const { std::lock_guard<std::mutex> l(x_minerWork); return m_work; }
        bool hasFoundBlock() const { return m_hasFoundBlock; }
        Solution m_solution;
        void setSolution(const Solution & solution) { m_solution = solution; }
        Solution getSolution() const { return m_solution; }
		/*std::chrono::steady_clock::time_point farmLaunched() {
			return m_farm_launched;
		}*/

/*
		string farmLaunchedFormatted() {
			auto d = std::chrono::steady_clock::now() - m_farm_launched;
			int hsize = 3;
			auto hhh = std::chrono::duration_cast<std::chrono::hours>(d);
			if (hhh.count() < 100) {
				hsize = 2;
			}
			d -= hhh;
			auto mm = std::chrono::duration_cast<std::chrono::minutes>(d);
			std::ostringstream stream;
			stream << "Time: " << std::setfill('0') << std::setw(hsize) << hhh.count() << ':' << std::setfill('0') << std::setw(2) << mm.count();
			return stream.str();
		}
*/

	private:
		/**
		 * @brief Called from a Miner to note a WorkPackage has a solution.
		 * @param _p The solution.
		 * @param _wp The WorkPackage that the Solution is for.
		 * @return true iff the solution was good (implying that mining should be .
		 */
		/*bool submitProof(Solution const& _s)
		{
            m_hasFoundBlock = true;
			//assert(m_onSolutionFound);
			//return m_onSolutionFound(_s);
		}*/

		mutable std::mutex x_minerWork;
		std::vector<std::shared_ptr<Miner>> m_miners;
		Work m_work;

		std::atomic<bool> m_isMining = {false};

		mutable std::mutex x_progress;
		//mutable WorkingProgress m_progress;

		SolutionFound m_onSolutionFound;
		MinerRestart m_onMinerRestart;
        bool m_hasFoundBlock;

		//std::map<std::string, SealerDescriptor> m_sealers;
		//std::string m_lastSealer;
		//bool b_lastMixed = false;

		//std::chrono::steady_clock::time_point m_lastStart;
		//int m_hashrateSmoothInterval = 10000;
		//std::thread m_serviceThread;  ///< The IO service thread.
		//boost::asio::io_service m_io_service;
		//boost::asio::deadline_timer * p_hashrateTimer = nullptr;
		//std::vector<WorkingProgress> m_lastProgresses;

		//mutable SolutionStats m_solutionStats;
		//std::chrono::steady_clock::time_point m_farm_launched = std::chrono::steady_clock::now();
	};


    class CPUMiner: public Miner
    {
    public:
        CPUMiner(Plant& _farm);
        ~CPUMiner()
        {}
    protected:
        //void kickOff() override;
        //void pause() override;
    private:
        void workLoop() override;
        //void report(uint64_t _nonce, Work const& _w);
        //bool init(const h256& seed);
    };

    class CLMiner: public Miner
    {
    public:
        CLMiner(Plant& _farm);
        ~CLMiner();
    protected:
        void kickOff() override;
        void pause() override;
    private:
        void workLoop() override;
        void report(uint64_t _nonce, Work const& _w);
        //bool init(const h256& seed);
    };
}

#endif /* ENERGIMINER_MINERCOMMON_H_ */
