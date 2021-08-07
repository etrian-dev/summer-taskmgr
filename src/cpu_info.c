#include "cpu_info.h"

#include <glib.h>
#include <stdio.h>

#include <unistd.h>

// gets statistics about the cpu usage
// see man 5 proc at section "/proc/stat"
gboolean get_cpu_info(
    gushort *cpu_count,
    gulong *user_time, gulong *user_time_prev,
    gulong *sys_time, gulong *sys_time_prev,
    gulong *tot_time, gulong *tot_time_prev) {

	sysconf(_SC_CLK_TCK);

	//temp local variables
	gushort num_cpus = 0;
	gulong usr = 0;
	gulong usr_lowprio = 0; // time spent in user mode with low priority
	gulong sys = 0;
	gulong idle = 0;

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

		*user_time_prev = *user_time;
		*sys_time_prev = *sys_time;
		*tot_time_prev = *tot_time;

		// calculate the output values based on the reads
		*cpu_count = num_cpus;
		*user_time = usr + usr_lowprio;
		*sys_time = sys;
		*tot_time = usr + usr_lowprio + sys + idle;
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
