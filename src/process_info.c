
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h> // to get usernames and user IDs from /etc/passwd

#include <glib.h>

#include "cpu_info.h"
#include "process_info.h"
#include "main.h"

// clears (but does not free) a Task structure (given as a pointer)
void clear_task(void *tp) {
    Task *task_ptr = (Task*)tp;
    if(task_ptr->command) free(task_ptr->command);
    if(task_ptr->username) free(task_ptr->username);
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
gboolean get_processes_info(TaskList *tasks, CPU_data_t *cpudata) {
    // sort the tasklist by increasing PID (and save the old sorting mode)
    // this expensive passage is needed to perform several faster binary searches on the array
    int (*oldsort)(const void*, const void*) = tasks->sortfun;
    g_array_sort(tasks->ps, cmp_pid_incr);

    // reset the number of threads, since each process could have changed its own
    // number of threads since the last time the data was updated
    tasks->num_threads = 0;
    // resets the presence flag of each process to mark it as terminated
    int i;
    for(i = 0; i < tasks->num_ps; i++) {
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
                memset(&newproc, 0, sizeof(Task));
                // get detailed process infos from the file above
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/stat", pid);
                gboolean stat_ret = get_stat_details(&newproc, path_statfile);
                // get the full command of this process (with options and args)
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/comm", pid);
                gboolean cmd_ret = get_cmdline(&newproc, path_statfile);
                // get a list of open file descriptors
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/fd", pid);
                gboolean fd_ok = get_open_fd(&newproc, path_statfile);
                // get the username and user id of this process's owner
                snprintf(path_statfile, BUF_BASESZ, "/proc/%ld/status", pid);
                gboolean set_uname = get_username(&newproc, path_statfile);


                unsigned int ps_idx;
                // If the process was already in the array just update its data
                if(g_array_binary_search(tasks->ps, &newproc, compare_PIDs, &ps_idx) == TRUE) {
                    Task *process = &g_array_index(tasks->ps, struct task, ps_idx);
                    // Update the task with new data, but leave PID, visibility and highlighting unchanged
                    process->ppid = newproc.ppid;
                    process->userid = newproc.userid;
                    if(process->username) {
                        free(process->username);
                    }
                    if(newproc.username) {
                        process->username = strdup(newproc.username);
                    }
                    if(process->command) {
                        free(process->command);
                        process->command = strdup(newproc.command);
                    }
                    if(process->open_fds) {
                        g_slist_free(process->open_fds);
                        process->open_fds = NULL;
                    }
                    process->open_fds = newproc.open_fds;
                    process->state = newproc.state;
                    process->cpu_usr = newproc.cpu_usr / cpudata->total.prev_total;
                    process->cpu_sys = newproc.cpu_sys / cpudata->total.prev_total;
                    process->nice = newproc.nice;
                    process->num_threads = newproc.num_threads;
                    process->virt_size_bytes = newproc.virt_size_bytes;
                    process->resident_set = newproc.resident_set;
                    // mark the updated process as still present in the system
                    process->present = TRUE;
                    // free fields in the new task since the old one has been updated
                    clear_task(&newproc);
                }
                else {
                    // process not found: insert it at the end of the new processes's array
                    // add to the process its list of open file decriptors
                    g_array_append_val(newprocs, newproc);
                    newprocs_sz += 1;
                }
            }
        }
        closedir(proc_dir);
        if(errno != 0) {
            // read error because errno changed
            return FALSE;
        }

        // Discard terminated processes (those still marked not present)
        i = 0;
        while(i < tasks->num_ps) {
            Task *t = &g_array_index(tasks->ps, Task, i);
            if(t->present == FALSE) {
                g_array_remove_index_fast(tasks->ps, i);
                // because of the implementation of the function above, the last item
                // in the array is used to fill the freed spot, so it must be examined
                // by not incrementing i in this iteration
                (tasks->num_ps)--;
                t = NULL;
            }
            else {
                // the process is still running, so count its number of threads
                tasks->num_threads += t->num_threads;
                i++;
            }
        }
        // Now merge the main process array and the one containing newly discovered processes
        for(i = 0; i < newprocs_sz; i++) {
            Task newt = g_array_index(newprocs, Task, i);
            g_array_append_val(tasks->ps, newt);
            // add in these threads as well
            tasks->num_threads += newt.num_threads;
        }
        // then update the number of processes
        tasks->num_ps += newprocs_sz;
        // the (now empty) new processes array can be freed
        g_array_free(newprocs, FALSE);

        // restore the old sorting criteria (but don't sort)
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
            // default flag values are: process present, visible and not highlighted
            proc->present = TRUE;
            proc->visible = TRUE;
            proc->highlight = FALSE;
            proc->pid = pid;
            proc->state = state;
            proc->ppid = ppid;
            proc->cpu_usr = usr_time * cpu_ticks_sec;
            proc->cpu_sys = sys_time * cpu_ticks_sec;
            proc->nice = nice;
            proc->num_threads = nthreads;
            proc->virt_size_bytes = vsize;
            proc->resident_set = rss;
        }
        fclose(fp);
    }
    return (fp ? TRUE : FALSE);
}

/**
 * \brief Reads the command line of this process
 *
 * Given a process whose PID is x, this function reads the file /proc/x/comm to
 * obtain the command name. If the string read from this file is 16 bytes long then it's
 * likely to have been truncated, so /proc/x/cmdline is used instead
 * command line (with arguments). The command line is truncated at BUF_BASESZ - 1 characters
 * \param [in,out] proc The pointer to the Task whose command line should be read
 * \param [in] cmd_filepath The path the the process's cmdline file (/proc/PID/cmdline)
 * \return Returns TRUE iff the command line has been read successfully and is not NULL, FALSE otherwise
 */
gboolean get_cmdline(Task *proc, const char *cmd_filepath) {
    // open the file containing the command line (either /proc/[pid]/comm or /proc/pid/cmdline)
    FILE *fp = fopen(cmd_filepath, "r");
    if(fp) {
        char *buf = NULL;
        if((buf = calloc(BUF_BASESZ, sizeof(char))) == NULL) {
            return FALSE;
        }
        // if the process has nothing in /proc/[pid]/comm then fallback to cmdline
        char *result = fgets(buf, BUF_BASESZ, fp);
        if(result == NULL) {
            free(buf);
            return FALSE;
        }
        // If the command is shorter than the max lenght (15 + null terminator)
        // then the comm hasn't been truncated. If a command's lenght is
        // greater or equal to that, then it's likely to have been truncated.
        // So, the full command is fetched trough /proc/[pid]/cmdline (if possible)
        // and it's used instead. If the full command is unavailable the truncated
        // command name is used instead
        if((strlen(buf) >= 15 || result == NULL) && strstr(cmd_filepath, "cmdline") == NULL) {
            char full_cmdline_path[BUF_BASESZ];
            snprintf(full_cmdline_path, BUF_BASESZ, "/proc/%d/cmdline", proc->pid);
            if(get_cmdline(proc, full_cmdline_path) == TRUE) {
                // the command line args are separated by NULLs. This loop substitutes each
                // of them with blanks (leaving the string terminator untouched)
                int i = 1;
                while(i < strlen(proc->command)) {
                    // two NULLs in a row mean the end of the string
                    if(proc->command[i-1] == '\0' && proc->command[i] == '\0') {
                        break;
                    }
                    // one NULL and then a non-NULL character mean an argument separator
                    if(proc->command[i-1] == '\0' && proc->command[i] != '\0') {
                        proc->command[i-1] = ' ';
                    }
                    i++;
                }
            }
        }
        // this branch is executed either if the command read from /proc/[pid]/comm
        // is shorter than 15 characters or is cmd_filepath is /proc/[pid]/cmdline
        else if(result != NULL) {
            proc->command = strdup(buf);
        }
        free(buf);
        fclose(fp);
    }
    return (fp ? TRUE : FALSE);
}
/**
 * \brief Get the list of this process's open file descriptor
 *
 * The function inserts in a list the names of open file descriptor associated to this
 * process, as read from /proc/PID/fd
 * \param [in,out] tp Pointer to the task structure to be updated with its open fd list
 * \param [in] fd_dir The path to the directory containing open fds as subdirectories
 * \return The list of open file descriptors iff at least one is open and can be read, NULL otherwise
 */
gboolean get_open_fd(Task *tp, const char *fd_dir) {
    if(tp->open_fds) {
        g_slist_free(tp->open_fds);
        tp->open_fds = NULL;
    }
    DIR *dirp = opendir(fd_dir);
    if(dirp) {
        struct dirent *entry = NULL;
        // no errno check, since i'm uninterested if some fds cannot be obtained
        while((entry = readdir(dirp)) != NULL) {
            long fd;
            if(isNumber(entry->d_name, &fd) == 0) {
                tp->open_fds = g_slist_prepend(tp->open_fds, GINT_TO_POINTER((int)fd));
            }
        }
        closedir(dirp);
    }
    return (dirp ? TRUE : FALSE);
}
/**
 * \brief Obtains the username of the user owning the process
 *
 * The process's status file contains the user id of the owner, thus its username can be obtained
 * by parsing the file /etc/passwd. This is accomplished by the library function getpwuid_r()
 */
gboolean get_username(Task *tp, const char *statusfile) {
    FILE *fp = fopen(statusfile, "r");
    char *buf = calloc(BUF_BASESZ, sizeof(char));
    if(fp && buf) {
        int uid = -1; // this process owner's (effective) user id
        while(fgets(buf, BUF_BASESZ, fp)) {
            // parse only the line containing "Uid"
            if(sscanf(buf, "Uid:\t%*d\t%d\t*d\t%*d", &uid) == 1) {
                // set the uid field of the task
                tp->userid = uid;
                break;
            }
        }
        fclose(fp);
        // Detect if the user id was actually parsed correctly
        if(uid == -1) {
            return FALSE;
        }
        // obtain the username of the user having this user id
        struct passwd pwd_entry;
        struct passwd *search_result;
        long suggested_size = sysconf(_SC_GETPW_R_SIZE_MAX);
        if(suggested_size == -1) {
            suggested_size = 16384; // 2^14, should be plenty
        }
        buf = realloc(buf, suggested_size);
        if(getpwuid_r(uid, &pwd_entry, buf, suggested_size, &search_result) == 0) {
            if(search_result != NULL) {
                // a matching entry in /etc/passwd has been found (and is contained in pwd_entry and *search_result)
                tp->username = strdup(pwd_entry.pw_name);
            }
        }
        // otherwise some error occurred in getting the username matching uid
        if(search_result == NULL) {
            // no matching userin /etc/passwd
            tp->username = NULL;
        }

        free(buf);
    }
    return (fp && tp->username ? TRUE : FALSE);
}



