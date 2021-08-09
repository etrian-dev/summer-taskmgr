#include "cpu_info.h"

#include <glib.h>
#include <stdio.h>

#include <unistd.h>

// gets statistics about the cpu usage
// see man 5 proc at section "/proc/stat"
gboolean get_cpu_info(int *num_cores, CPU_data_t *cpudata) {
	// gets the unit used to express CPU time reported by /proc/stat
	// usually it's 100, meaning that one needs to scale up by 100 to get the real value
	gulong USER_HZ = sysconf(_SC_CLK_TCK); // runtime constant

	//temp local variables
	gushort num_cpus = 0;
	gulong usr = 0;
	gulong usr_lowprio = 0; // time spent in user mode with low priority
	gulong sys = 0;
	gulong idle = 0;
	gulong total = 0;

	FILE *stat_file = NULL;
	char *buf = malloc(BUF_BASESZ);
	gulong bufsz = BUF_BASESZ;
	if((stat_file = fopen(CPU_STATFILE, "r")) && buf) {
		if(fgets(buf, bufsz, stat_file) == NULL) {
			free(buf);
			return FALSE;
		}
		// only the first four fields are relevant: others are just ignored
		// if the values are not read properly they still can be obtained from the sum of cores
		sscanf(buf, "cpu %lu %lu %lu %lu %*[^\n]\n", &usr, &usr_lowprio, &sys, &idle);
		while(fgets(buf, bufsz, stat_file) && buf[0] == 'c' && buf[1] == 'p' && buf[2] == 'u') {
			num_cpus++;
		}
		fclose(stat_file);


		// calculate the output values based on the reads
		*num_cores = num_cpus;
		total = usr + usr_lowprio + sys + idle;
		// calculate the new values using deltas
		float fact = USER_HZ / (float)(total - cpudata->prev_total);
		cpudata->perc_usr = (usr - cpudata->prev_usr) * fact;
		cpudata->perc_usr_nice = (usr_lowprio - cpudata->prev_usr_nice) * fact;
		cpudata->perc_sys = (sys - cpudata->prev_sys) * fact;
		cpudata->perc_idle = (idle - cpudata->prev_idle) * fact;
		// update the previous values with the current reads
		cpudata->prev_usr = usr;
		cpudata->prev_usr_nice = usr_lowprio;
		cpudata->prev_sys = sys;
		cpudata->prev_idle = idle;
		cpudata->prev_total = total;
	}
	return (stat_file && buf ? TRUE : FALSE);
}

// utility function to obtain the cpu usage percentage for the value passes as the first param
// the formula is (curr - prev) / (tot_current - tot_prev) iff curr > prev, 0 otherwise
gfloat calc_percentage(gulong current, gulong prev, gulong tot_current, gulong tot_prev) {
	gfloat div = tot_current - tot_prev;
	if(current <= prev || div <= 0.0) {
		return 0.0;
	}
	return 100.0 * (current - prev) / div;
}
