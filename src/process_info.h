#ifndef PROCESS_INFO_DEFINED
#define PROCESS_INFO_DEFINED

#include <glib.h>

#define PROC_DIR "/proc"
#define BUF_BASESZ 256

struct task {
    gboolean visible; // flag used to hide the process from the view
    gboolean present; // flag used to indicate that the process was found in the last scan
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

struct tasklist {
    long int num_ps;
    long int num_threads;
    GArray *ps;
    // syncronization variables
    pthread_mutex_t mux_memdata;
    pthread_cond_t cond_updating;
    gboolean is_busy;
    int (*sortfun)(const void*, const void*);
};
typedef struct tasklist TaskList;

// clears (but does not free) a Task structure (given as a pointer)
void clear_task(void *tp);


// switch between sorting modes
void switch_sortmode(TaskList *tasks, int (*newmode)(const void *, const void *));
// Process sorting functions
// lexicographical sorting on the cmdline string
int cmp_commands(const void *a, const void *b);
// pid increasing sorting
int cmp_pid_incr(const void *a, const void *b);
// pid decreasing sorting
int cmp_pid_decr(const void *a, const void *b);

// gets information about the running processes
gboolean get_processes_info(TaskList *tasks);
gboolean get_stat_details(Task *proc, const char *stat_filepath);
gboolean get_cmdline(Task *proc, const char *cmd_filepath);

int isNumber(const char* s, long* n);

#endif
