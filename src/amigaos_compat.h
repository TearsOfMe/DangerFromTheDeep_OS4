#ifndef DFTD_AMIGAOS_COMPAT_H
#define DFTD_AMIGAOS_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__amigaos4__)

/* Query if the program was launched from Workbench (no CLI attached) */
int amigaos_is_launched_from_workbench(void);

/* Get current process name (returns empty string or name if available) */
const char *amigaos_get_process_name(void);

/* Detect AltiVec via ExecSG GetCPUInfoTags.
 * Returns 1 if AltiVec present, 0 otherwise.
 * If out_model is non-NULL, stores CPU model string pointer.
 * If out_vector_unit is non-NULL, stores raw vector unit code.
 */
int amigaos_detect_altivec(const char **out_model, unsigned int *out_vector_unit);

/* Return pointer to current Exec Task (struct Task *) as void * */
void *amigaos_get_current_task(void);

/* Return task name for a given task pointer */
const char *amigaos_get_task_name(void *task);

/* Signal a task with SIGBREAKF_CTRL_C and enter infinite Wait(0) loop */
void amigaos_signal_task_and_wait(void *target_task) __attribute__((noreturn));

/* Wait forever (used in child threads when halting/terminating) */
void amigaos_wait_forever(void) __attribute__((noreturn));

/* Iterate ExecBase LibList and log loaded libraries via callback */
typedef void (*amigaos_log_cb)(const char *fmt, ...);
void amigaos_log_loaded_libs(amigaos_log_cb log_func);

#endif /* __amigaos4__ */

#ifdef __cplusplus
}
#endif

#endif /* DFTD_AMIGAOS_COMPAT_H */
