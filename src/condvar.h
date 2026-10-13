/*
  Danger from the Deep - Open source submarine simulation
  Copyright (C) 2003-2006  Thorsten Jordan, Luis Barrancos and others.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// multithreading primitives: condition variable
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifndef CONDVAR_H
#define CONDVAR_H

#include "mutex.h"
#ifdef __amigaos4__
#include <SDL.h>
#else
#include <condition_variable>
#endif

/// A classical condition variable.
///@note Condition variables work together with class mutex.
class condvar {
  protected:
#ifdef __amigaos4__
    SDL_cond *cdv;
#else
    std::condition_variable_any cdv;
#endif

  private:
    condvar(const condvar &) = delete;
    condvar &operator=(const condvar &) = delete;

  public:
    /// create condition variable
#ifdef __amigaos4__
    condvar() : cdv(SDL_CreateCond()) {}

    /// destroy condition variable
    ~condvar() {
        if (cdv) {
            SDL_DestroyCond(cdv);
            cdv = nullptr;
        }
    }

    /// wait on condition
    ///@param m - mutex to encapsulate waiting condition
    void wait(::mutex &m) {
        if (cdv && m.mtx) {
            SDL_CondWait(cdv, m.mtx);
        }
    }

    /// wait on condition with timeout
    ///@param m - mutex to encapsulate waiting condition
    ///@param ms - timeout value in milliseconds
    ///@return true when woken up by signal, false on timeout
    bool timed_wait(::mutex &m, unsigned ms) {
        if (cdv && m.mtx) {
            return SDL_CondWaitTimeout(cdv, m.mtx, ms) == 0;
        }
        return false;
    }

    /// send signal to threads that are waiting on the condition.
    void signal() {
        if (cdv) SDL_CondBroadcast(cdv);
    }
#else
    condvar() = default;

    /// destroy condition variable
    ~condvar() = default;

    /// wait on condition
    ///@param m - mutex to encapsulate waiting condition
    void wait(::mutex &m) {
        cdv.wait(m.mtx);
    }

    /// wait on condition with timeout
    ///@param m - mutex to encapsulate waiting condition
    ///@param ms - timeout value in milliseconds
    ///@return true when woken up by signal, false on timeout
    bool timed_wait(::mutex &m, unsigned ms) {
        auto result = cdv.wait_for(m.mtx, std::chrono::milliseconds(ms));
        return result != std::cv_status::timeout;
    }

    /// send signal to threads that are waiting on the condition.
    ///@note Note that before sending the signal you must make the condition false that would
    ///	let the other thread call the wait(), or race conditions can occour.
    ///	Code that changes the condition must be encapsulated in a mutex-lock with the SAME mutex
    ///	that is used for the wait.\n
    ///	So typically call lock(); cond = false; signal(); unlock();
    void signal() {
        cdv.notify_all();
    }
#endif
};

#endif
