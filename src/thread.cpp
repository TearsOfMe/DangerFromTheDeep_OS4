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

// multithreading primitives: thread
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "error.h"
#include "log.h"
#include "system.h"
#include "thread.h"

#ifdef __amigaos4__
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

int thread::thread_entry_sdl(void *arg) {
    thread *t = static_cast<thread *>(arg);
    try {
        t->run();
    } catch (const std::exception &e) {
        dftd_startup_log("thread_entry_sdl: Exception in thread '%s': %s", t ? t->myname.c_str() : "null", e.what());
        if (t) t->thread_error_message = e.what();
    } catch (...) {
        dftd_startup_log("thread_entry_sdl: Unknown exception in thread '%s'", t ? t->myname.c_str() : "null");
        if (t) t->thread_error_message = "UNKNOWN";
    }
    return 0;
}
#endif

void thread::thread_entry(thread *arg) {
    thread *t = arg;
    try {
        t->run();
    } catch (const std::exception &e) {
        dftd_startup_log("thread_entry: Exception in thread '%s': %s", t ? t->myname.c_str() : "null", e.what());
        t->thread_error_message = e.what();
    } catch (...) {
        dftd_startup_log("thread_entry: Unknown exception in thread '%s'", t ? t->myname.c_str() : "null");
        t->thread_error_message = "UNKNOWN";
    }
}

thread::thread(const char *name)
    : thread_obj_ptr(nullptr),
#ifdef __amigaos4__
      thread_sdl_ptr(nullptr),
#endif
      thread_abort_request(false),
      thread_state(THRSTAT_NONE),
      myname(name ? name : "unnamed") {
    if (name == nullptr)
        throw error("thread name is null");
}

void thread::run() {
    try {
        log::instance().new_thread(myname.c_str());
        init();
    } catch (std::exception &e) {
        // failed to initialize, report that
        dftd_startup_log("thread::run: Exception during init of thread '%s': %s", myname.c_str(), e.what());
        mutex_locker ml(thread_state_mutex);
        thread_error_message = e.what();
        thread_state.store(THRSTAT_INIT_FAILED);
        thread_start_cond.signal();
        return;
    } catch (...) {
        // failed to initialize, report that
        dftd_startup_log("thread::run: Unknown exception during init of thread '%s'", myname.c_str());
        mutex_locker ml(thread_state_mutex);
        thread_error_message = "UNKNOWN";
        thread_state.store(THRSTAT_INIT_FAILED);
        thread_start_cond.signal();
        return;
    }
    // initialization was successfully, report that
    {
        mutex_locker ml(thread_state_mutex);
        thread_state.store(THRSTAT_RUNNING);
        thread_start_cond.signal();
    }
    try {
        while (!abort_requested()) {
            loop();
        }
        deinit();
        log::instance().end_thread();
    } catch (std::exception &e) {
        // thread execution failed
        dftd_startup_log("thread::run: Exception in loop of thread '%s': %s", myname.c_str(), e.what());
        mutex_locker ml(thread_state_mutex);
        thread_error_message = e.what();
        thread_state.store(THRSTAT_ABORTED);
        return;
    } catch (...) {
        // thread execution failed
        dftd_startup_log("thread::run: Unknown exception in loop of thread '%s'", myname.c_str());
        mutex_locker ml(thread_state_mutex);
        thread_error_message = "UNKNOWN";
        thread_state.store(THRSTAT_ABORTED);
        return;
    }
    // normal execution finished
    mutex_locker ml(thread_state_mutex);
    thread_state.store(THRSTAT_FINISHED);
}

thread::~thread() {
#ifdef __amigaos4__
    // thread_sdl_ptr is cleaned up in join()
#else
    if (thread_obj_ptr) {
        delete thread_obj_ptr;
        thread_obj_ptr = nullptr;
    }
#endif
}

void thread::request_abort() {
    thread_abort_request.store(true);
}

void thread::start() {
    if (thread_abort_request.load())
        throw error("thread abort requested, but start() called");
    mutex_locker ml(thread_state_mutex);

    if (thread_state.load() != THRSTAT_NONE)
        throw error("thread already started, but start() called again");

#ifdef __amigaos4__
    thread_sdl_ptr = SDL_CreateThread(thread_entry_sdl, myname.c_str(), this);
    if (!thread_sdl_ptr) {
        dftd_startup_log("thread::start: SDL_CreateThread failed for '%s': %s", myname.c_str(), SDL_GetError());
        throw error(std::string("thread start failed: ") + SDL_GetError());
    }
#else
    thread_obj_ptr = new std::thread(thread_entry, this);
    if (!thread_obj_ptr->joinable()) {
        delete thread_obj_ptr;
        thread_obj_ptr = nullptr;
        throw error("thread start failed");
    }
#endif
    // we could wait with timeout, but how long? init could take any time...

    thread_start_cond.wait(thread_state_mutex);

    // now check if thread has started
    if (thread_state.load() == THRSTAT_INIT_FAILED)
        throw std::runtime_error(("thread start failed: ") + thread_error_message);
    // very rare, but possible
    else if (thread_state.load() == THRSTAT_ABORTED)
        throw std::runtime_error(("thread run failed: ") + thread_error_message);
}

void thread::join() {
#ifdef __amigaos4__
    if (thread_sdl_ptr) {
        int status = 0;
        SDL_WaitThread(thread_sdl_ptr, &status);
        thread_sdl_ptr = nullptr;
    }
#else
    if (thread_obj_ptr && thread_obj_ptr->joinable()) {
        thread_obj_ptr->join();
    }
#endif
    std::string error_msg = thread_error_message; // Copy before delete
    delete this;
    if (!error_msg.empty())
        throw error(std::string("thread aborted with error: ") + error_msg);
}

void thread::destruct() {
    thread_state_t ts = thread_state.load();
    // request if thread runs, in that case send abort request
    if (ts == THRSTAT_RUNNING)
        request_abort();
    // request if thread has ever run, in that case we need to join
    if (thread_state.load() != THRSTAT_NONE)
        join();
    else
        delete this;
}

void thread::sleep(unsigned ms) {
#ifdef __amigaos4__
    SDL_Delay(ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

thread::id thread::get_my_id() {
#ifdef __amigaos4__
    return static_cast<uint64_t>(SDL_ThreadID());
#else
    // Convert std::thread::id to uint64_t
    std::thread::id tid = std::this_thread::get_id();
    std::ostringstream oss;
    oss << tid;
    return std::stoull(oss.str());
#endif
}

thread::id thread::get_id() const {
#ifdef __amigaos4__
    if (thread_sdl_ptr) {
        return static_cast<uint64_t>(SDL_GetThreadID(thread_sdl_ptr));
    }
    return 0;
#else
    if (thread_obj_ptr) {
        std::thread::id tid = thread_obj_ptr->get_id();
        std::ostringstream oss;
        oss << tid;
        return std::stoull(oss.str());
    }
    return 0;
#endif
}

bool thread::is_running() {
    // only reading is normally safe, but not for multi-core architectures.
    mutex_locker ml(thread_state_mutex);
    return thread_state.load() == THRSTAT_RUNNING;
}
