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
    MutexLGuard l(mutex_work_);
    cdebug << "start working" << name_;

    // If a valid thread exist then set state
    if (tworker_)
    {
      if ( state_ == State::Stopped ) // start
      {
        state_ = State::Starting;
      }
    }
    else // first time launch
    {
      state_ = State::Starting;
      tworker_.reset(new std::thread([&]()
      {
        while (state_ != State::Killing)
        {
          if ( state_ == State::Starting )
          {
            state_ = State::Started;
          }


          try
          {
            trun();
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
          else if ( state_ == State::Stopping )
          {
            state_ = State::Stopped;
          }

          // Its like waiting to be woken up, unless killed
          while (state_ == State::Stopped)
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
          }
        }
      }));
    }


    // Wait for thread to switch to started state
    while (state_ == State::Starting)
    {
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    return true;
  }

  bool Worker::stop()
  {
    MutexLGuard l(mutex_work_);
    if (tworker_)
    {
      if ( state_ != State::Stopped )
      {
        state_ = State::Stopping;
      }

      while (state_ != State::Stopped)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(20));
      }
    }

    return true;
  }

  void Worker::setWork(const Work& work)
  {
    MutexLGuard l(mutex_work_);
    work_ = work;

    onSetWork();
  }

  Worker::~Worker()
  {
    MutexLGuard l(mutex_work_);
    if (tworker_)
    {
      state_ = State::Killing;
      tworker_->join();
      tworker_.reset();
    }
  }
} /* namespace energi */
