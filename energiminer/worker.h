/*
 * Worker.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef ENERGIMINER_WORKER_H_
#define ENERGIMINER_WORKER_H_

#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <cstdlib>

#include "energiminer/common.h"
#include "energiminer/work.h"

namespace energi
{
  class Worker
  {
  public:
    enum class State : unsigned char
    {
      Starting = 0,
      Started = 1,
      Stopping = 2,
      Stopped = 3,
      Killing = 4
    };

  public:
    Worker(const std::string& name)
    : name_(name)
    {}

    Worker& operator=(const Worker &) = default;
    virtual ~Worker();

    bool start(); // starts a Worker thread
    bool stop();  // stops  a Worker thread

    bool isStopped() const
    {
      return state_ != State::Started;
    }

    // Since work is assigned to the worked by the plant and Worker is doing the real
    // work in a thread, protect the data carefully.
    void setWork(const Work& work);
    Work work()
    {
      MutexLGuard l(mutex_work_);
      return work_;
    }
  protected:

    virtual void onSetWork(){}
    // run in a thread
    // This function is meant to run some logic in a loop.
    // Should quit once done or when new work is assigned

    virtual void trun(){}

  private:

    std::string                   name_;
    Work                          work_; // Dont expose this, instead allow it to be copied
    State                         state_ { State::Starting };
    mutable std::mutex            mutex_work_; // protext work since its actually used in a thread
    std::unique_ptr<std::thread>  tworker_;
  };

} /* namespace energi */

#endif /* ENERGIMINER_WORKER_H_ */
