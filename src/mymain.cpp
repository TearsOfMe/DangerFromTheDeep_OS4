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

// Danger from the Deep (C)+(W) Thorsten Jordan. SEE LICENSE
//
// system dependent main
//
// WIN32: use WinMain, divide cmd line to list of strings
// UNIX: C-like main, make strings from char** array
// MacOSX: ? fixed with objective C code ?
//

#include "faulthandler.h"
#include "log.h"
#include "system.h"
#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <unistd.h>
using namespace std;

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>

#if defined(DFTD_NO_DEBUG_LOG)
void dftd_set_debug_log(bool) {}
void dftd_set_in_game_loop(bool) {}
bool dftd_is_debug_log_enabled() { return false; }
void dftd_startup_log(const char *, ...) {}
#else
static bool s_debug_log_enabled = false;
static bool s_in_game_loop = false;

void dftd_set_debug_log(bool enabled) {
    s_debug_log_enabled = enabled;
}

void dftd_set_in_game_loop(bool in_loop) {
    s_in_game_loop = in_loop;
}

bool dftd_is_debug_log_enabled() {
    return s_debug_log_enabled;
}

void dftd_startup_log(const char *fmt, ...) {
    if (!fmt) return;

    if (!s_debug_log_enabled) {
        bool is_critical = (fmt[0] == 'F' && strncmp(fmt, "FATAL", 5) == 0) ||
                           (fmt[0] == 'C' && strncmp(fmt, "CRITICAL", 8) == 0) ||
                           (fmt[0] == 'S' && strncmp(fmt, "Stack trace", 11) == 0) ||
                           (fmt[0] == ' ' && fmt[1] == ' ' && fmt[2] == '[') ||
                           (strncmp(fmt, "game__exec: fps", 15) == 0) ||
                           (strncmp(fmt, "CPU Vector Unit", 15) == 0) ||
                           (strncmp(fmt, "[CPU", 4) == 0);
        if (!is_critical && s_in_game_loop) {
            return;
        }
    }

    static FILE *logfile = nullptr;
    static bool tried_open = false;
    if (!tried_open) {
        tried_open = true;
#if defined(__amigaos4__)
        logfile = fopen("PROGDIR:dftd_startup.log", "a");
        if (!logfile)
            logfile = fopen("dftd_startup.log", "a");
#else
        logfile = fopen("dftd_startup.log", "a");
#endif
    }
    char buf[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);

    printf("%s\n", buf);
    fflush(stdout);
    if (logfile) {
        fprintf(logfile, "%s\n", buf);
        fflush(logfile);
#if defined(__amigaos4__) || defined(__unix__)
        fsync(fileno(logfile));
#endif
    }
}
#endif

int mymain(list<string> &args);

int call_mymain(list<string> &args) {
#if !defined(DFTD_NO_DEBUG_LOG)
    for (const auto &arg : args) {
        if (arg == "--debug" || arg == "-debug") {
            s_debug_log_enabled = true;
        }
    }
    const char *env_debug = getenv("DFTD_DEBUG");
    if (env_debug && (strcmp(env_debug, "1") == 0 || strcmp(env_debug, "true") == 0)) {
        s_debug_log_enabled = true;
    }
#endif

    install_segfault_handler();
    dftd_startup_log("=== Danger from the Deep starting ===");
    for (const auto &arg : args) {
        dftd_startup_log("Arg: %s", arg.c_str());
    }

#ifndef __amigaos4__
    std::set_terminate([]() {
        dftd_startup_log("FATAL: std::terminate called! Uncaught C++ exception!");
        std::exception_ptr p = std::current_exception();
        if (p) {
            try {
                std::rethrow_exception(p);
            } catch (const std::exception &e) {
                dftd_startup_log("FATAL: Uncaught exception: %s", e.what());
            } catch (...) {
                dftd_startup_log("FATAL: Uncaught non-std exception!");
            }
        }
        abort();
    });
#endif

    log_info("***** Log file started *****");
    int result = 0;
    try {
        result = mymain(args);
    } catch (const system::quit_exception& qe) {
        dftd_startup_log("Normal quit exception (code %d)", qe.retval);
        result = qe.retval;
    } catch (const std::exception& e) {
        dftd_startup_log("CRITICAL EXCEPTION: %s", e.what());
#if !defined(DFTD_NO_DEBUG_LOG)
        std::cout << "CRITICAL ERROR: Caught exception: " << e.what() << std::endl;
        std::cerr << "CRITICAL ERROR: Caught exception: " << e.what() << std::endl;
        printf("CRITICAL ERROR: Caught exception: %s\n", e.what());
#endif
        log_warning("Caught exception: " << e.what());
        result = -1;
    } catch (...) {
        dftd_startup_log("CRITICAL UNKNOWN EXCEPTION");
#if !defined(DFTD_NO_DEBUG_LOG)
        std::cout << "CRITICAL ERROR: Caught unknown exception" << std::endl;
        std::cerr << "CRITICAL ERROR: Caught unknown exception" << std::endl;
        printf("CRITICAL ERROR: Caught unknown exception\n");
#endif
        result = -2;
    }

#if !defined(DFTD_NO_DEBUG_LOG)
    string log_file =
#if defined(__amigaos4__)
        "PROGDIR:debug.log";
#elif defined(WIN32)
        "./debug.log";
#else
        (getenv("HOME") ? (string(getenv("HOME")) + "/.dangerdeep/debug.log") : "./debug.log");
#endif
    log::instance().write(std::cerr, log::LOG_SYSINFO);
    unlink(log_file.c_str());
    std::ofstream f(log_file.c_str());
    log::instance().write(f, log::LOG_SYSINFO);
#endif
    log::destroy_instance();
#if !defined(DFTD_NO_DEBUG_LOG)
    std::cout.flush();
    std::cerr.flush();
    fflush(stdout);
    fflush(stderr);
    dftd_startup_log("=== Danger from the Deep exiting (code %d) ===", result);
#endif
    return result;
}

#ifdef WIN32

std::cout << "Should not be here!" << std::endl;

int WinMain(HINSTANCE, HINSTANCE, LPSTR cmdline, int) {
    string mycmdline(cmdline);
    list<string> args;
    // parse mycmdline
    while (mycmdline.length() > 0) {
        string::size_type st = mycmdline.find(" ");
        args.push_back(mycmdline.substr(0, st));
        if (st == string::npos)
            break;
        mycmdline = mycmdline.substr(st + 1);
    }
    return call_mymain(args);
}

#else // UNIX

int main(int argc, char *argv[]) {

    list<string> args;
    // parse argc, argv, do not store program name
    while (argc > 1) {
        // std::cout << "args: " << string(argv[argc]) << std::endl;
        args.push_front(string(argv[--argc]));
    }
    return call_mymain(args);
}

#endif
