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

    Worker& operator=(const Worker &) = default;
    virtual ~Worker();

    virtual bool start(); // starts a Worker thread
    bool stop();  // stops  a Worker thread
    void stopAllWork();

    bool shouldStop() const
    {
        return m_state == State::Stopping || m_state == State::Stopped;
    }

    // Since work is assigned to the worked by the plant and Worker is doing the real
    // work in a thread, protect the data carefully.
    void setWork(const Work& work);

    Work getWork()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_work;
    }

    void updateWorkTimestamp()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_work.updateTimestamp();
    }

    std::string name() const
    {
        return m_name;
    }
protected:

    virtual void onSetWork(){}
    // run in a thread
    // This function is meant to run some logic in a loop.
    // Should quit once done or when new work is assigned

    virtual void trun(){}

private:

    std::string                   m_name;
    Work                          m_work; // Dont expose this, instead allow it to be copied
    std::atomic<State>            m_state { State::Starting };
    mutable std::recursive_mutex  m_mutex; // protext work since its actually used in a thread
    std::unique_ptr<std::thread>  m_threadWorker;
};

} //! namespace energi

#endif /* ENERGIMINER_WORKER_H_ */
