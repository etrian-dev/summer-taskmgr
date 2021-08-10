
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <glib.h>

#include "process_info.h"

// clears (but does not free) a Task structure (given as a pointer)
void clear_task(void *tp) {
    Task *task_ptr = (Task*)tp;
    free(task_ptr->command);
}

// gets information about the running processes
gboolean get_processes_info(GArray *processes, long *num_ps, long *num_threads) {
    // open the directory stream containing processes in the system as subdirs
    DIR *proc_dir = opendir(PROC_DIR);
    gchar path_statfile[BUF_BASESZ]; // this buffer will hold the path to the stat file of each process

    *num_ps = 0; // the number of processes currently in the system
    *num_threads = 0; // the total number of threads (of all processes) currently in the system

    if(proc_dir) {
        // read the dir stream
        struct dirent *entry = NULL;
        errno = 0; // reset to distinguish read errors from the end of the directory
        while((entry = readdir(proc_dir))) {
            // process only directories whose name is an integer (files like /proc/[pid])
            long int pid;
            if(entry->d_type == DT_DIR && isNumber(entry->d_name, &pid) == 0) {
                // a new process has been found: add it to the task array
                Task newproc;
                newproc.visible = TRUE; // by default a process is visible

                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/stat", pid);
                // get detailed process infos from the file above
                gboolean stat_ret = get_stat_details(&newproc, path_statfile);
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/cmdline", pid);
                // get the full command of this process (with options and args)
                gboolean cmd_ret = get_cmdline(&newproc, path_statfile);
                if(stat_ret && cmd_ret) {
                    g_array_append_val(processes, newproc);
                    (*num_ps)++;
                    *num_threads += newproc.num_threads;
                }
                else {
                    if(newproc.command) free(newproc.command);
                }
            }
        }
        if(errno != 0) {
            // read error because errno changed
            return FALSE;
        }

        closedir(proc_dir);
    }
    return (proc_dir != NULL ? TRUE : FALSE);
}

gboolean get_stat_details(Task *proc, const char *stat_filepath) {
    FILE *fp = fopen(stat_filepath, "r");
    char buf[BUF_BASESZ];
    long cpu_ticks_sec = sysconf(_SC_CLK_TCK); // get the clock ticks per second

    // fields contained in the stat file
    int pid, ppid, exit_status;
    long int nice, nthreads;
    long int rss; // # of pages of this process in real memory (the resident set) [not reliable]
    long unsigned int usr_time, sys_time, vsize;
    char state;

    if(fp) {
        if(fgets(buf, BUF_BASESZ, fp)) {
            sscanf(buf,
                   /*
                    * from /proc/pid/stat's documentation
                    * 1st row: fields 1 to 18. 2nd row: fiels 19 to 34. 3rd row: fields 35 to 52
                    */
                   "%d (%*[^ ] %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d\
	     %ld %ld %*d %*[0-9] %lu %ld %*[0-9] %*u %*u %*u %*u %*u %*u %*u %*u %*u\
	     %*u %*u %*u %*d %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %d",
                   &pid, &state, &ppid, &usr_time, &sys_time, &nice, &nthreads, &vsize, &rss, &exit_status);

            // fill the task structure with reads
            proc->pid = pid;
            proc->state = state;
            proc->ppid = ppid;
            proc->cpu_usr = usr_time / cpu_ticks_sec;
            proc->cpu_sys = sys_time / cpu_ticks_sec;
            proc->nice = nice;
            proc->num_threads = nthreads;
            proc->virt_size_bytes = vsize;
            proc->resident_set = rss;
        }
        fclose(fp);
    }
    return (fp ? TRUE : FALSE);
}

gboolean get_cmdline(Task *proc, const char *cmd_filepath) {
    FILE *fp = fopen(cmd_filepath, "r");
    char buf[BUF_BASESZ];
    memset(buf, '\0', BUF_BASESZ * sizeof(char));
    if(fp) {
        if(fgets(buf, BUF_BASESZ, fp)) {
            // the command line args are separated by nulls instead of blanks, so I substitute them
            // to make it more readable
            for(int i = 0; i < BUF_BASESZ - 1; i++) {
                if(buf[i] == '\0' && buf[i+1] != '\0') {
                    buf[i] = ' ';
                }
            }
            proc->command = strdup(buf);
        }
        else {
            proc->command = NULL;
        }
        fclose(fp);
    }
    return (fp && proc->command ? TRUE : FALSE);
}

/**
 * Funzione per convertire una stringa s in un long int.
 * isNumber ritorna:
 * - 0: conversione ok
 * - 1: non e' un numbero
 * - 2: overflow/underflow
 */
int isNumber(const char* s, long* n) {
    if (s==NULL) return 1;
    if (strlen(s)==0) return 1;
    char* e = NULL;
    errno=0;
    long val = strtol(s, &e, 10);
    if (errno == ERANGE) return 2;    // overflow
    if (e != NULL && *e == (char)0) {
        *n = val;
        return 0;   // successo
    }
    return 1;   // non e' un numero
}


