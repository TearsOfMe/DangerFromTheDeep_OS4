#include "altivec_detect.h"
#include "log.h"
#include <SDL2/SDL_cpuinfo.h>
#include <cstdio>

#if defined(__amigaos4__)
#include <proto/exec.h>
#include <exec/exectags.h>
#endif

bool cpu_has_altivec() {
    static int s_cached = -1;
    if (s_cached != -1) {
        return s_cached != 0;
    }

    bool has = false;
    const char *detected_via = "None";

#if defined(__amigaos4__)
    if (IExec) {
        uint32 vector_unit = VECTORTYPE_NONE;
        CONST_STRPTR model_str = NULL;
        IExec->GetCPUInfoTags(
            GCIT_ModelString, &model_str,
            GCIT_VectorUnit, &vector_unit,
            TAG_DONE);

        printf("\n======================================================\n");
        printf("[CPU-DETECT] AmigaOS ExecSG CPU Info:\n");
        printf("[CPU-DETECT]   CPU Model: %s\n", model_str ? (const char *)model_str : "Unknown PPC");
        printf("[CPU-DETECT]   VectorUnit: %u (ALTIVEC constant: %u)\n",
               (unsigned)vector_unit, (unsigned)VECTORTYPE_ALTIVEC);

        if (vector_unit == VECTORTYPE_ALTIVEC) {
            has = true;
            detected_via = "AmigaOS4 ExecSG GCIT_VectorUnit";
        }
    }
#endif

    // Fallback or secondary verification via SDL2
    SDL_bool sdl_vec = SDL_HasAltiVec();
    printf("[CPU-DETECT] SDL_HasAltiVec() reported: %s\n", sdl_vec == SDL_TRUE ? "YES" : "NO");

    if (!has && sdl_vec == SDL_TRUE) {
        has = true;
        detected_via = "SDL_HasAltiVec()";
    }

    if (has) {
        printf("[CPU-DETECT] RESULT: AltiVec / VMX detected via %s!\n", detected_via);
        printf("[CPU-DETECT] -> Hardware-accelerated SIMD routines ACTIVE.\n");
    } else {
        printf("[CPU-DETECT] RESULT: No AltiVec detected.\n");
        printf("[CPU-DETECT] -> Running in standard scalar mode.\n");
    }
    printf("======================================================\n\n");
    fflush(stdout);

    s_cached = has ? 1 : 0;
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
