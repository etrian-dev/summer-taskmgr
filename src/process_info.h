#ifndef PROCESS_INFO_DEFINED
#define PROCESS_INFO_DEFINED

#include <glib.h>

#define PROC_DIR "/proc"
#define BUF_BASESZ 256

struct task {
    glong pid;
    guint ppid;
    gchar *command;
    gchar state;
    gulong cpu_usr;
    gulong cpu_sys;
    glong num_threads;
};
typedef struct task Task;

// gets information about the running processes
gboolean get_processes_info(GArray **processes, gulong *num_ps, gulong *num_threads);
gboolean get_stat_details(Task *proc, const char *stat_filepath);
int isNumber(const char* s, long* n);

#endif
