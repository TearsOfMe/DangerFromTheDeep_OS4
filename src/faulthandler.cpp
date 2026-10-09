#include "faulthandler.h"

#if defined(__amigaos4__)

#include "log.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <proto/exec.h>
#include <dos/dos.h>
#if __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#elif __has_include(<SDL_mixer.h>)
#include <SDL_mixer.h>
#endif

static struct Task *s_main_task = NULL;

static void amiga_safe_audio_stop() {
    static bool audio_stopped = false;
    if (audio_stopped) return;
    audio_stopped = true;
    dftd_startup_log("faulthandler: Stopping SDL audio...");
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    SDL_PauseAudio(1);
    Mix_CloseAudio();
    dftd_startup_log("faulthandler: SDL audio stopped.");
}

static void amiga_terminate_handler() {
    struct Task *cur_task = IExec ? IExec->FindTask(NULL) : NULL;
    const char *tname = (cur_task && cur_task->tc_Node.ln_Name) ? cur_task->tc_Node.ln_Name : "unknown";
    dftd_startup_log("FATAL: std::terminate() called in task '%s' (0x%p, main=0x%p)!", tname, cur_task, s_main_task);
    std::exception_ptr p = std::current_exception();
    if (p) {
        try {
            std::rethrow_exception(p);
        } catch (const std::exception &e) {
            dftd_startup_log("FATAL: Uncaught std::exception: %s", e.what());
        } catch (...) {
            dftd_startup_log("FATAL: Uncaught unknown C++ exception!");
        }
    } else {
        dftd_startup_log("FATAL: std::terminate called without active exception.");
    }
    if (s_main_task && cur_task != s_main_task) {
        dftd_startup_log("FATAL: terminate in child thread, signaling main task to terminate...");
        IExec->Signal(s_main_task, SIGBREAKF_CTRL_C);
        for (;;) {
            IExec->Wait(0);
        }
    } else {
        amiga_safe_audio_stop();
        exit(128 + SIGABRT);
    }
}

extern "C" void __assert(const char *file, int line, const char *failedexpr) {
    struct Task *cur_task = IExec ? IExec->FindTask(NULL) : NULL;
    const char *tname = (cur_task && cur_task->tc_Node.ln_Name) ? cur_task->tc_Node.ln_Name : "unknown";
    dftd_startup_log("FATAL: Assertion failed: (%s) at %s:%d in task '%s' (0x%p)!",
                     failedexpr ? failedexpr : "null",
                     file ? file : "unknown",
                     line, tname, cur_task);
    if (s_main_task && cur_task != s_main_task) {
        IExec->Signal(s_main_task, SIGBREAKF_CTRL_C);
        for (;;) {
            IExec->Wait(0);
        }
    } else {
        amiga_safe_audio_stop();
        exit(128 + SIGABRT);
    }
}

static uintptr_t s_elf_ref_addr = 0x011a9be0;

void print_stack_trace() {
    uintptr_t ref_addr = (uintptr_t)(void*)&install_segfault_handler;
    intptr_t load_bias = ref_addr - s_elf_ref_addr;
    dftd_startup_log("Stack trace (most recent call first), load_bias=0x%08lx:", (unsigned long)load_bias);

    void **sp = (void**)__builtin_frame_address(0);
    int depth = 0;
    while (sp && depth < 30) {
        void **next_sp = (void**)*sp;
        void *caller_lr = (next_sp && next_sp > sp) ? *(next_sp + 1) : NULL;
        void *raw_lr = *(sp + 1);
        uintptr_t elf_lr = caller_lr ? ((uintptr_t)caller_lr - load_bias) : 0;
        dftd_startup_log("  [%2d] sp=0x%p caller_lr=0x%p (elf=0x%08lx) raw_lr=0x%p",
                         depth, (void*)sp, caller_lr, (unsigned long)elf_lr, raw_lr);
        if (!next_sp || next_sp <= sp)
            break;
        sp = next_sp;
        depth++;
    }
}

extern "C" void abort(void) {
    struct Task *cur_task = IExec ? IExec->FindTask(NULL) : NULL;
    const char *tname = (cur_task && cur_task->tc_Node.ln_Name) ? cur_task->tc_Node.ln_Name : "unknown";
    dftd_startup_log("FATAL: abort() was called directly in task '%s' (0x%p)!", tname, cur_task);
    print_stack_trace();
    if (s_main_task && cur_task != s_main_task) {
        dftd_startup_log("FATAL: abort() in child thread! Signaling main task to terminate...");
        IExec->Signal(s_main_task, SIGBREAKF_CTRL_C);
        for (;;) {
            IExec->Wait(0);
        }
    } else {
        amiga_safe_audio_stop();
        exit(128 + SIGABRT);
    }
}

static void amiga_break_handler(int sig) {
    struct Task *cur_task = IExec ? IExec->FindTask(NULL) : NULL;
    const char *tname = (cur_task && cur_task->tc_Node.ln_Name) ? cur_task->tc_Node.ln_Name : "unknown";
    dftd_startup_log("faulthandler: Break/Interrupt signal %d received in task '%s' (0x%p, main=0x%p)...",
                     sig, tname, cur_task, s_main_task);
    if (s_main_task && cur_task != s_main_task) {
        dftd_startup_log("faulthandler: Break/Interrupt in child thread '%s', suspending without exit()...", tname);
        for (;;) {
            IExec->Wait(0);
        }
        return;
    }
    amiga_safe_audio_stop();
    exit(0);
}

static void amiga_fault_handler(int sig) {
    const char *name = "UNKNOWN";
    if (sig == SIGSEGV) name = "SIGSEGV (Segmentation Fault)";
    else if (sig == SIGFPE) name = "SIGFPE (Floating Point Exception)";
    else if (sig == SIGILL) name = "SIGILL (Illegal Instruction)";
    else if (sig == SIGABRT) name = "SIGABRT (Abort)";

    struct Task *cur_task = IExec ? IExec->FindTask(NULL) : NULL;
    const char *tname = (cur_task && cur_task->tc_Node.ln_Name) ? cur_task->tc_Node.ln_Name : "unknown";
    dftd_startup_log("FATAL: Caught signal %d [%s] in task '%s' (0x%p, main=0x%p)!",
                     sig, name, tname, cur_task, s_main_task);
    print_stack_trace();
    signal(sig, SIG_DFL);
    if (s_main_task && cur_task != s_main_task) {
        dftd_startup_log("FATAL: Signal caught in child thread '%s'! Signaling main task to terminate...", tname);
        IExec->Signal(s_main_task, SIGBREAKF_CTRL_C);
        for (;;) {
            IExec->Wait(0);
        }
    } else {
        amiga_safe_audio_stop();
        exit(128 + sig);
    }
}

void install_segfault_handler() {
    s_main_task = IExec ? IExec->FindTask(NULL) : NULL;
    uintptr_t ref_addr = (uintptr_t)(void*)&install_segfault_handler;
    intptr_t load_bias = ref_addr - s_elf_ref_addr;
    dftd_startup_log("faulthandler: installed, main task is 0x%p ('%s'), &install_segfault_handler=0x%p (bias=0x%08lx)",
                     s_main_task, (s_main_task && s_main_task->tc_Node.ln_Name) ? s_main_task->tc_Node.ln_Name : "unknown",
                     (void*)&install_segfault_handler, (unsigned long)load_bias);

    if (SysBase) {
        struct ExecBase *eb = (struct ExecBase *)SysBase;
        IExec->Forbid();
        for (struct Node *node = eb->LibList.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ) {
            dftd_startup_log("faulthandler: loaded lib '%s' at %p", node->ln_Name ? node->ln_Name : "unnamed", (void*)node);
        }
        IExec->Permit();
    }

    atexit(amiga_safe_audio_stop);
    std::set_terminate(amiga_terminate_handler);
    signal(SIGSEGV, amiga_fault_handler);
    signal(SIGFPE, amiga_fault_handler);
    signal(SIGILL, amiga_fault_handler);
    signal(SIGABRT, amiga_fault_handler);
    signal(SIGINT, amiga_break_handler);
    signal(SIGTERM, amiga_break_handler);
}

#elif !((defined(__APPLE__) && defined(__MACH__)) || defined(MINGW32) || defined(WIN32))

#include <cxxabi.h> // Needed for __cxa_demangle
#include <execinfo.h>
#include <iostream>
#include <list>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

void print_stack_trace() {

    void *array[16];
    int size = backtrace(array, 16);
    cout << "size: " << size << endl;
    if (size < 0) {
        fprintf(stderr, "Backtrace could not be created!\n");
        return;
    }
    char **strings = backtrace_symbols(array, size);
    if (!strings) {
        fprintf(stderr, "Could not get Backtrace symbols!\n");
        return;
    }

    fprintf(stderr, "Stack trace: (%u frames)\n", size);
    std::string addrs;
    std::list<std::string> lines;
    for (int i = 0; i < size; ++i) {
        std::string addr;
        std::string s(strings[i]);
        std::string::size_type p1 = s.rfind('[');
        std::string::size_type p2 = s.rfind(']');
        if ((p1 != std::string::npos) && (p2 != std::string::npos)) {
            addr = s.substr(p1 + 1, p2 - p1 - 1);
            addrs += addr + " ";
        }
        p1 = s.rfind("_Z");
        p2 = s.rfind('+');
        if (p2 == std::string::npos)
            p2 = s.rfind(')');
        std::string func;
        if (p1 != std::string::npos) {
            func = s.substr(p1, p2 - p1);
            int status = 0;
            char *c = abi::__cxa_demangle(func.c_str(), 0, 0, &status);
            if (c)
                func = c;
            else
                func = "???";
        } else {
            p1 = s.rfind('(');
            if (p1 != std::string::npos) {
                func = s.substr(p1 + 1, p2 - p1 - 1);
            } else {
                p2 = s.rfind('[');
                func = s.substr(0, p2 - 1);
            }
        }
        lines.push_back(addr + " in " + func);
        if (func == "main")
            break;
    }
    free(strings);

    // determine lines from addresses
    std::ostringstream oss;
    oss << "addr2line -e /proc/" << getpid() << "/exe -s " << addrs;
    FILE *f = popen(oss.str().c_str(), "r");
    if (f) {
        for (std::list<std::string>::iterator it = lines.begin(); it != lines.end(); ++it) {
            char tmp[128];
            if (NULL == fgets(tmp, 127, f))
                break;
            fprintf(stderr, "%s at %s", it->c_str(), tmp);
        }
        pclose(f);
    } else {
        for (std::list<std::string>::iterator it = lines.begin(); it != lines.end(); ++it) {
            fprintf(stderr, "%s\n", it->c_str());
        }
    }
}

void sigsegv_handler(int number) {

    fprintf(stderr, "SIGSEGV caught!\n");
    print_stack_trace();
    fprintf(stderr, "Aborting program.\n");
    abort();
}

void install_segfault_handler() {

    signal(SIGSEGV, sigsegv_handler);
}

#endif
