#ifndef ALTIVEC_DETECT_H
#define ALTIVEC_DETECT_H

// Cached flag for zero-overhead inlined branch checks
extern bool g_cpu_has_altivec;
extern bool g_cpu_altivec_initialized;

bool cpu_detect_altivec_init();

inline bool cpu_has_altivec() {
    if (__builtin_expect(!g_cpu_altivec_initialized, 0)) {
        return cpu_detect_altivec_init();
    }
    return g_cpu_has_altivec;
}

// Logs CPU vector capabilities to startup log and standard log
void log_cpu_features();

#endif // ALTIVEC_DETECT_H
