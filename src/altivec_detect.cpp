#include "altivec_detect.h"
#include "log.h"
#include <SDL2/SDL_cpuinfo.h>
#include <cstdio>

#if defined(__amigaos4__)
#include "amigaos_compat.h"
#endif

bool g_cpu_has_altivec = false;
bool g_cpu_altivec_initialized = false;

bool cpu_detect_altivec_init() {
    if (g_cpu_altivec_initialized) {
        return g_cpu_has_altivec;
    }

    bool has = false;
#if !defined(DFTD_NO_DEBUG_LOG)
    const char *detected_via = "None";
#endif

#if defined(__amigaos4__)
    const char *model_str = NULL;
    unsigned int vector_unit = 0;
    int is_vec = amigaos_detect_altivec(&model_str, &vector_unit);

#if !defined(DFTD_NO_DEBUG_LOG)
    printf("\n======================================================\n");
    printf("[CPU-DETECT] AmigaOS ExecSG CPU Info:\n");
    printf("[CPU-DETECT]   CPU Model: %s\n", model_str ? model_str : "Unknown PPC");
    printf("[CPU-DETECT]   VectorUnit: %u (ALTIVEC constant: 1)\n", vector_unit);
#endif

    if (is_vec) {
        has = true;
#if !defined(DFTD_NO_DEBUG_LOG)
        detected_via = "AmigaOS4 ExecSG GCIT_VectorUnit";
#endif
    }
#endif

    // Fallback or secondary verification via SDL2
    SDL_bool sdl_vec = SDL_HasAltiVec();
#if !defined(DFTD_NO_DEBUG_LOG)
    printf("[CPU-DETECT] SDL_HasAltiVec() reported: %s\n", sdl_vec ? "YES" : "NO");
#endif

    if (!has && sdl_vec) {
        has = true;
#if !defined(DFTD_NO_DEBUG_LOG)
        detected_via = "SDL_HasAltiVec()";
#endif
    }

#if !defined(DFTD_NO_DEBUG_LOG)
    if (has) {
        printf("[CPU-DETECT] RESULT: AltiVec / VMX detected via %s!\n", detected_via);
        printf("[CPU-DETECT] -> Hardware-accelerated SIMD routines ACTIVE.\n");
    } else {
        printf("[CPU-DETECT] RESULT: No AltiVec detected.\n");
        printf("[CPU-DETECT] -> Running in standard scalar mode.\n");
    }
    printf("======================================================\n\n");
    fflush(stdout);
#endif

    g_cpu_has_altivec = has;
    g_cpu_altivec_initialized = true;
    return has;
}

void log_cpu_features() {
    bool has_vec = cpu_has_altivec();
    const char *status_str = has_vec ? "AltiVec / VMX ENABLED (hardware acceleration ACTIVE)"
                                     : "None / standard scalar mode (AltiVec OFF)";
    dftd_startup_log("CPU Vector Unit: %s", status_str);
    dftd_startup_log("[CPU] SIMD acceleration: %s", has_vec ? "ACTIVE" : "OFF");
    log_info("CPU Vector Unit: " << status_str);
}
