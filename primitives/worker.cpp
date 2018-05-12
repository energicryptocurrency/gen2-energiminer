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

bool Worker::start()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    cdebug << "Worker spawning a thread to get its job done" << m_name;
    if (m_threadWorker) {
        if ( m_state == State::Stopped ) { // start only if already stopped else let it continue
            m_state = State::Starting;
        }
    } else  { // Launch thread first time
        m_state = State::Starting;
        m_threadWorker.reset(new std::thread([&]() {
            while (m_state != State::Killing) {// let thread spin till its killed
                if ( m_state == State::Starting ) {
                    m_state = State::Started;
                    try {
                        trun(); // real computational work happens here
                        m_state = State::Stopped;
                        //cnote << " Out of trun" << name();
                    } catch (const std::exception& ex) {
                        std::cout << "Worker thread: Exception thrown: " << m_name  << ex.what();
                        std::cout.flush();
                    }
                    if ( m_state == State::Killing ) {// Pre check: what if we directly kill, we want to break then
                        break;
                    }
                    //cnote << " Last State " << static_cast<int>(m_state.load()) << name();
                }
                // Its like waiting to be woken up, unless killed
                while (m_state == State::Stopped) {
                    // TODO: real wait not sleep
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
        }));
    }
    // Wait for thread to switch to started state after start
    while (m_state == State::Starting) {
        // TODO: real wait not sleep
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
    return true;
}

bool Worker::stop()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_threadWorker) {
        //cnote << " State now" << m_name << static_cast<int>(m_state.load());
        if ( m_state != State::Stopped ) {
            m_state = State::Stopping;
            // TODO: should be a proper wait/signal not a sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            //cnote << " Waiting here" << m_name << static_cast<int>(m_state.load());
            m_state = State::Stopped;
        }
    }
    return true;
}

void Worker::setWork(const Work& work)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_work = work;
    m_work.incrementExtraNonce();
    m_state = State::Starting;

    onSetWork();
}

void Worker::stopAllWork()
{
    stop();
}

Worker::~Worker()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_threadWorker) {
        m_state = State::Killing;
        m_threadWorker->join();
        m_threadWorker.reset();
    }
}
