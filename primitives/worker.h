/*
 * Worker.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_WORKER_H_
#define ENERGIMINER_WORKER_H_

#include <atomic>
#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <cstdlib>

#include "common/common.h"
#include "work.h"

namespace energi {

class Worker
{
public:
    enum class State : int
    {
        Starting = 0,
        Started = 1,
        Stopping = 2,
        Stopped = 3,
        Killing = 4
    };

public:
    Worker(const std::string& name)
        : m_name(name)
    {}

    Worker(Worker const&) = delete;
    Worker& operator=(Worker const&) = delete;
    virtual ~Worker();

    virtual void startWorking(); // starts a Worker thread
    void stopWorking();  // stops  a Worker thread

    bool shouldStop() const
    {
        return m_state == State::Stopping || m_state == State::Stopped;
    }

    std::string name() const
    {
        return m_name;
    }
protected:
    // run in a thread
    // This function is meant to run some logic in a loop.
    // Should quit once done or when new work is assigned
    virtual void trun() = 0;

private:

    std::string                   m_name;
    mutable std::mutex            x_work;
    std::unique_ptr<std::thread>  m_work;

    std::atomic<State>            m_state { State::Starting };
};

} //! namespace energi

#endif /* ENERGIMINER_WORKER_H_ */
