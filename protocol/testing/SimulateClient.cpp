#include <chrono>

#include "SimulateClient.h"

using namespace energi;

SimulateClient::SimulateClient(const unsigned* difficulty, const unsigned& block)
    : PoolClient()
    , Worker("simulator")
    , m_difficulty(difficulty - 1)
    , m_block(block)
{
}

SimulateClient::~SimulateClient()
{
}

void SimulateClient::connect()
{
    m_connected.store(true, std::memory_order_relaxed);
    m_uppDifficulty = false;

    if (m_onConnected) {
        m_onConnected();
    }
    startWorking();
}

void SimulateClient::disconnect()
{
    m_connected.store(false, std::memory_order_relaxed);

    if (m_onDisconnected) {
        m_onDisconnected();
    }
}

void SimulateClient::submitHashrate(const std::string& rate)
{
    (void)rate;

    auto sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_time);
    cnote << "On difficulty" << m_difficulty << " for " << sec.count() << " seconds";
}

void SimulateClient::submitSolution(const Solution& solution)
{
    m_uppDifficulty = true;
    cnote << "Difficulty: " << m_difficulty;

    const auto& work = solution.getWork();
    auto hash = Miner::GetPOWHash(work);
    if(UintToArith256 <= work.hashTarget) {
        if (m_onSolutionAccepted) {
            m_onSolutionAccepted(false);
        }
    } else {
        if (m_onSolutionRejected) {
            m_onSolutionRejected(false);
        }
    }
}

void SimulateClient::trun()
{
    cnote << "Preparing DAG for block #" << m_block;
    BlockHeader genesis;
    genesis.setNumber(m_block);
    WorkPackage current = WorkPackage(genesis);
    m_time = std::chrono::steady_clock::now();
    while (true) {
        if (m_connected.load(std::memory_order_relaxed)) {
            if (m_uppDifficulty) {
                m_uppDifficulty = false;

                auto sec = duration_cast<seconds>(steady_clock::now() - m_time);
                cnote << "Took" << sec.count() << "seconds at" << m_difficulty
                    << "difficulty to find solution";

                if (sec.count() < 12) {
                    ++m_difficulty;
                }
                if (sec.count() > 18) {
                    --m_difficulty;
                }

                cnote << "Now using difficulty " << m_difficulty;
                m_time = std::chrono::steady_clock::now();
                if (m_onWorkReceived) {
                    genesis.setDifficulty(u256(1) << m_difficulty);
                    genesis.noteDirty();

                    current.header = h256::random();
                    current.boundary = genesis.boundary();

                    m_onWorkReceived(current);
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        } else {
            this_thread::sleep_for(chrono::seconds(5));
        }
    }
}

