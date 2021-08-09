#ifndef CPU_INFO_INCLUDED
#define CPU_INFO_INCLUDED

#include <glib.h>

#define CPU_STATFILE "/proc/stat"
#define BUF_BASESZ 256

// defines a structure to hold cpu statistics
typedef struct _cpu_data_t {
    // previous (unscaled) data points
    unsigned long int prev_usr;
    unsigned long int prev_usr_nice;
    unsigned long int prev_sys;
    unsigned long int prev_idle;
    unsigned long int prev_total;
    // current cpu usage percentages (calculated on deltas)
    float perc_usr;
    float perc_usr_nice;
    float perc_sys;
    float perc_idle;
} CPU_data_t;

// gets statistics about the cpu usage
// not reentrant: uses static variables
gboolean get_cpu_info(int *num_cores, CPU_data_t *cpudata);

// utility function to obtain the cpu usage percentage for the value passes as the first param
// the formula is (curr - prev) / (tot_current - tot_prev) iff curr > prev, 0 otherwise
gfloat calc_percentage(gulong current, gulong prev, gulong tot_current, gulong tot_prev);

#endif
