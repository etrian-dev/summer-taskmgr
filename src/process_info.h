#ifndef PROCESS_INFO_DEFINED
#define PROCESS_INFO_DEFINED

#include <glib.h>

#define PROC_DIR "/proc"
#define BUF_BASESZ 256

struct task {
    gboolean visible; // flag used to hide the process from the view
    int pid;
    char *command;
    char state;
    int ppid;
    gulong cpu_usr;
    gulong cpu_sys;
    long int nice; // process priority (nice value): ranges from 19 (low prio) to -20 (high prio)
    long int num_threads;
    long int virt_size_bytes; // size of the virtual memory occupied by the process (in bytes)
    long int resident_set; // the number of pages of this process in physical memory at the moment (unreliable)
};
typedef struct task Task;

// clears (but does not free) a Task structure (given as a pointer)
void clear_task(void *tp);
// filters tasks that contain the string filter in their commandline
// all unmatched tasks are hidden
void filter_tasks(GArray *tasks, const char *filter);

// gets information about the running processes
gboolean get_processes_info(GArray *processes, long *num_ps, long *num_threads);
gboolean get_stat_details(Task *proc, const char *stat_filepath);
gboolean get_cmdline(Task *proc, const char *cmd_filepath);

int isNumber(const char* s, long* n);

#endif