/*
 * Worker.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#include <iostream>

#include "worker.h"
#include "common/Log.h"


using namespace energi;

void Worker::startWorking()
{
    std::lock_guard<std::mutex> lock(x_work);
    if (m_work) {
        State ex = State::Stopped;
        m_state.compare_exchange_strong(ex, State::Starting);
    } else {
        m_state = State::Starting;
        m_work.reset(new std::thread([&]() {
                while (m_state != State::Killing) {
                    State ex = State::Starting;
                    m_state.compare_exchange_strong(ex, State::Started);
                    try {
                        trun();
                    } catch (std::exception const& _e) {
                        clog(WarnChannel) << "Exception thrown in Worker thread: " << _e.what();
                    }
                    ex = m_state.exchange(State::Stopped);
                    if (ex == State::Killing || ex == State::Starting) {
                        m_state.exchange(ex);
                    }
                    while (m_state == State::Stopped) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
               }
        }));
    }
    while (m_state == State::Starting) {
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
}

void Worker::stopWorking()
{
    DEV_GUARDED(x_work)
    if (m_work) {
        State ex = State::Started;
        m_state.compare_exchange_strong(ex, State::Stopping);

        while (m_state != State::Stopped) {
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    }
}

Worker::~Worker()
{
    DEV_GUARDED(x_work)
    if (m_work) {
        m_state.exchange(State::Killing);
        m_work->join();
        m_work.reset();
    }
}
