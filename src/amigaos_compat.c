#if defined(__amigaos4__)

#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/exectags.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include "amigaos_compat.h"

/* Disable newlib's automatic CON: window creation when launched from Workbench */
const char *__stdiowin = "NIL:";

int amigaos_is_launched_from_workbench(void) {
    if (!IExec) return 0;
    struct Task *self_task = IExec->FindTask(NULL);
    struct Process *self_proc = (struct Process *)self_task;
    if (self_proc && self_proc->pr_CLI == 0) {
        return 1;
    }
    return 0;
}

const char *amigaos_get_process_name(void) {
    if (!IExec) return "";
    struct Task *self_task = IExec->FindTask(NULL);
    if (self_task && self_task->tc_Node.ln_Name) {
        return self_task->tc_Node.ln_Name;
    }
    return "";
}

int amigaos_detect_altivec(const char **out_model, unsigned int *out_vector_unit) {
    if (!IExec) return 0;
    uint32 vector_unit = VECTORTYPE_NONE;
    CONST_STRPTR model_str = NULL;
    IExec->GetCPUInfoTags(
        GCIT_ModelString, &model_str,
        GCIT_VectorUnit, &vector_unit,
        TAG_DONE);

    if (out_model) {
        *out_model = model_str ? (const char *)model_str : "Unknown PPC";
    }
    if (out_vector_unit) {
        *out_vector_unit = (unsigned int)vector_unit;
    }
    return (vector_unit == (uint32)VECTORTYPE_ALTIVEC) ? 1 : 0;
}

void *amigaos_get_current_task(void) {
    return IExec ? (void *)IExec->FindTask(NULL) : NULL;
}

const char *amigaos_get_task_name(void *task) {
    if (!task) return "unknown";
    struct Task *t = (struct Task *)task;
    return (t->tc_Node.ln_Name) ? t->tc_Node.ln_Name : "unknown";
}

void amigaos_signal_task_and_wait(void *target_task) {
    if (IExec && target_task) {
        IExec->Signal((struct Task *)target_task, SIGBREAKF_CTRL_C);
    }
    if (IExec) {
        for (;;) {
            IExec->Wait(0);
        }
    }
}

void amigaos_wait_forever(void) {
    if (IExec) {
        for (;;) {
            IExec->Wait(0);
        }
    }
}

void amigaos_log_loaded_libs(amigaos_log_cb log_func) {
    if (!log_func || !IExec || !SysBase) return;
    struct ExecBase *eb = (struct ExecBase *)SysBase;
    IExec->Forbid();
    for (struct Node *node = eb->LibList.lh_Head; node->ln_Succ != NULL; node = node->ln_Succ) {
        log_func("faulthandler: loaded lib '%s' at %p", node->ln_Name ? node->ln_Name : "unnamed", (void*)node);
    }
    IExec->Permit();
}

#endif /* __amigaos4__ */
