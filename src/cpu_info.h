#ifndef CPU_INFO_INCLUDED
#define CPU_INFO_INCLUDED

#include <glib.h>

#define CPU_STATFILE "/proc/stat"
#define CPU_MODELFILE "/proc/cpuinfo" // arch-dependent contents
#define BUF_BASESZ 256

// defines a structure to hold cpu statistics
typedef struct cpu_data_t {
    char *model;
    int num_cores;
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
    // syncronization variables
    pthread_mutex_t mux_memdata;
    pthread_cond_t cond_updating;
    gboolean is_busy;
} CPU_data_t;

// gets statistics about the cpu usage
// not reentrant: uses static variables
gboolean get_cpu_info(CPU_data_t *cpudata);
gboolean get_cpu_model(char **model, int *cores);

#endif
