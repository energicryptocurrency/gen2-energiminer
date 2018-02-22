/*
 * Worker.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include "worker.h"
#include "Log.h"

#include <iostream>


namespace energi
{
  bool Worker::start()
  {
    MutexRLGuard l(mutex_work_);
    cdebug << "Worker spawning a thread to get its job done" << name_;
    if (tworker_)
    {
      if ( state_ == State::Stopped ) // start only if already stopped else let it continue
      {
        state_ = State::Starting;
      }
    }
    else // Launch thread first time
    {
      state_ = State::Starting;
      tworker_.reset(new std::thread([&]()
      {
        while (state_ != State::Killing) // let thread spin till its killed
        {
          if ( state_ == State::Starting )
          {
            state_ = State::Started;

            try
            {
              trun(); // real computational work happens here
              state_ = State::Stopped;
              cnote << " Out of trun" << name();
            }
            catch (std::exception const& _e)
            {
              std::cout << "Worker thread: Exception thrown: " << name_  << _e.what();
              std::cout.flush();
            }

            if ( state_ == State::Killing ) // Pre check: what if we directly kill, we want to break then
            {
              break;
            }

            cnote << " Last State " << (int)state_ << name();

          }

          // Its like waiting to be woken up, unless killed
          while (state_ == State::Stopped)
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
          }
        }
      }));
    }


    // Wait for thread to switch to started state after start
    while (state_ == State::Starting)
    {
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    return true;
  }

  bool Worker::stop()
  {
    MutexRLGuard l(mutex_work_);
    if (tworker_)
    {
      cnote << " State now" << name_ << (int)state_;
      if ( state_ != State::Stopped )
      {
        state_ = State::Stopping;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        cnote << " Waiting here" << name_ << (int)state_;
        state_ = State::Stopped;
      }
    }

    return true;
  }

  void Worker::setWork(const Work& work)
  {
    MutexRLGuard l(mutex_work_);
    work_ = work;
    state_ = State::Starting;

    onSetWork();
  }


  void Worker::stopAllWork()
  {
    stop();
  }


  Worker::~Worker()
  {
    MutexRLGuard l(mutex_work_);
    if (tworker_)
    {
      state_ = State::Killing;
      tworker_->join();
      tworker_.reset();
    }
  }
} /* namespace energi */
