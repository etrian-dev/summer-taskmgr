
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
    if(task_ptr->command) free(task_ptr->command);
}

int compare_PIDs(const void *a, const void *b) {
    Task *ta = (Task*)a;
    Task *tb = (Task*)b;
    return ta->pid - tb->pid;
}

/**
 * \brief Obtains updated information about processes executing in the system
 *
 * This function updates the TaskList given with information about the processes currently
 * executing on this machine. It does so by reading the contents of /proc to gather
 * command lines, PIDs, etc... Updates are performed based on the difference with the
 * previous list of tasks to improve efficiency
 * \param [in,out] tasks The structure holding (among other things) the array of processes in the system
 * \return Returns TRUE iff the update was completed successfully, FALSE otherwise
 */
gboolean get_processes_info(TaskList *tasks) {
    // sort the tasklist by increasing PID (and save the old sorting mode)
    // this expensive passage is needed to perform several faster binary searches on the array
    int (*oldsort)(const void*, const void*) = tasks->sortfun;
    g_array_sort(tasks->ps, cmp_pid_incr);
    // resets the presence flag of each process to mark it as terminated
    for(int i; i < tasks->num_ps; i++) {
        Task *t = &g_array_index(tasks->ps, Task, i);
        t->present = FALSE;
    }
    // new processes started between the past update and now are inserted in this (unsorted) array
    GArray *newprocs = g_array_new(FALSE, FALSE, sizeof(Task));
    int newprocs_sz = 0;

    // open the directory stream containing processes in the system as subdirs
    DIR *proc_dir = opendir(PROC_DIR);
    gchar path_statfile[BUF_BASESZ]; // this buffer will hold the path to the stat file of each process
    if(proc_dir) {
        struct dirent *entry = NULL;
        // reset to distinguish read errors from the end of the directory as both situations
        // make the readdir() function return NULL and thus exit the loop
        errno = 0;
        while((entry = readdir(proc_dir))) {
            long int pid;
            // ignore files that are not directories and directories whose name
            // is not an integer (the complete path must be /proc/pid)
            if(entry->d_type == DT_DIR && isNumber(entry->d_name, &pid) == 0) {
                Task newproc;
                // get detailed process infos from the file above
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/stat", pid);
                gboolean stat_ret = get_stat_details(&newproc, path_statfile);
                // get the full command of this process (with options and args)
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/cmdline", pid);
                gboolean cmd_ret = get_cmdline(&newproc, path_statfile);
                if(stat_ret && cmd_ret) {
                    unsigned int ps_idx;
                    // If the process was already in the array just update its data
                    if(g_array_binary_search(tasks->ps, &newproc, compare_PIDs, &ps_idx)) {
                        // Update the task with new data (actually the PID and visibility are the same)
                        Task *process = &g_array_index(tasks->ps, struct task, ps_idx);
                        process->present = TRUE;
                        process->ppid = newproc.ppid;
                        process->command = newproc.command;
                        process->state = newproc.state;
                        process->cpu_usr = newproc.cpu_usr;
                        process->cpu_sys = newproc.cpu_sys;
                        process->nice = newproc.nice;
                        process->num_threads = newproc.num_threads;
                        process->virt_size_bytes = newproc.virt_size_bytes;
                        process->resident_set = newproc.resident_set;
                    }
                    else {
                        // process not found: insert it at the end of the new processes's array
                        newproc.visible = TRUE; // by default a process is visible
                        g_array_append_val(newprocs, newproc);
                        newprocs_sz += 1;
                    }
                }
                else {
                    // cannot read the process information properly: discard it
                    if(newproc.command) free(newproc.command);
                }
            }
        }
        closedir(proc_dir);
        if(errno != 0) {
            // read error because errno changed
            return FALSE;
        }

        // Discard terminated processes
        int i = 0;
        while(i < tasks->num_ps) {
            Task *t = &g_array_index(tasks->ps, Task, i);
            if(t->present == FALSE) {
                g_array_remove_index_fast(tasks->ps, i);
                // because of the implementation of the function above, the last item
                // in the array is used to fill the freed spot, so it must be examined
                // by not incrementing i in this iteration
                (tasks->num_ps)--;
                tasks->num_threads -= t->num_threads;
                clear_task(t);
                t = NULL;
            }
            else {
                i++;
            }
        }
        // Now merge the main process array and the one containing newly discovered processes
        for(i = 0; i < newprocs_sz; i++) {
            Task newt = g_array_index(newprocs, Task, i);
            g_array_append_val(tasks->ps, newt);
            // add its threads to the count
            tasks->num_threads += newt.num_threads;
        }
        // then update the number of processes
        tasks->num_ps += newprocs_sz;
        // the (now empty) new processes array can be freed
        g_array_free(newprocs, FALSE);

        // restore the old sorting criteria
        tasks->sortfun = oldsort;
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
            proc->present = TRUE; // process found
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


