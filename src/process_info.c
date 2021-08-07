
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <glib.h>

#include "process_info.h"

// gets information about the running processes
gboolean get_processes_info(GArray **processes, gulong *num_ps, gulong *num_threads) {
    // open the directory stream containing processes in the system as subdirs
    DIR *proc_dir = opendir(PROC_DIR);
    gchar proc_stat[BUF_BASESZ];

    *processes = g_array_new(FALSE, TRUE, sizeof(Task));
    *num_ps = 0; // the number of processes (may be < than the size of the array)
    *num_threads = 0; // the total number of threads (of all processes)

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
		snprintf(proc_stat, BUF_BASESZ, "/proc/%ld/stat", pid);
		// get detailed process infos from the file above
		if(get_stat_details(&newproc, proc_stat)) {
		    g_array_append_val(*processes, newproc);
		    (*num_ps)++;
		    (*num_threads) += newproc.num_threads;
		}
	    }
	}

	closedir(proc_dir);
    }
    return (processes != NULL ? TRUE : FALSE);
}

gboolean get_stat_details(Task *proc, const char *stat_filepath) {
    FILE *fp = fopen(stat_filepath, "r");
    char buf[BUF_BASESZ];
    long cpu_ticks_sec = sysconf(_SC_CLK_TCK); // get the clock ticks per second

    // fields contained in the stat file
    int pid, ppid, exit_status;
    long int nice, nthreads;
    long int resident_set; // # of pages of this process in real memory (the resident set) [UNRELIABLE]
    long unsigned int usr_time, sys_time, virt_size;
    char cmd[BUF_BASESZ], state;
    memset(cmd, '\0', BUF_BASESZ * sizeof(char));

    if(fp) {
	if(fgets(buf, BUF_BASESZ, fp)) {
	    sscanf(buf,
	    "%d (%[^)]) %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d\
	     %ld %ld %*d %*u %lu %ld %*u %*u %*u %*u %*u %*u %*u %*u %*u\
	     %*u %*u %*u %*u %*d %*u %*u %*u %*u %*d %*u %*u %*u %*u %*u\
	     %*u %*u %d",
	    &pid, cmd, &state, &ppid, &usr_time, &sys_time, &nice, &nthreads, &virt_size, &resident_set, &exit_status);

	    proc->pid = pid;
	    proc->ppid = ppid; // fix
	    proc->command = strdup(cmd);
	    proc->state = state;
	    proc->cpu_usr = usr_time / cpu_ticks_sec; // fix
	    proc->cpu_sys = sys_time / cpu_ticks_sec; // fix
	    proc->num_threads = nthreads; // fix
	}
	fclose(fp);
    }
    return (fp ? TRUE : FALSE);
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


