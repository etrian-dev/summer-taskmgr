/**
 * \file cpu_info.c
 * \brief File containing functions that query the system for updated CPU information
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include "cpu_info.h"

// calculates the cpu usage percentage from the current and previous data points
float get_percentage(unsigned long curr, unsigned long prev, unsigned long tot_curr, unsigned long tot_prev, unsigned long scale)
{
    if (tot_curr == tot_prev)
    {
        return (float)(curr - prev) * scale / tot_curr;
    }
    return (float)(curr - prev) * scale / (tot_curr - tot_prev);
}

// gets statistics about the cpu usage
// see man 5 proc at section "/proc/stat"
bool get_cpu_info(CPU_data_t *cpudata)
{
    // gets the unit used to express CPU time reported by /proc/stat
    // usually it's 100, meaning that one needs to scale up by 100 to get the real value
    unsigned long USER_HZ = sysconf(_SC_CLK_TCK); // runtime constant

    // temp local variables
    unsigned long usr = 0;
    unsigned long usr_lowprio = 0; // time spent in user mode with low priority
    unsigned long sys = 0;
    unsigned long idle = 0;
    unsigned long total = 0;

    int running_processes = 0;

    FILE *stat_file = NULL;
    char *buf = malloc(BUF_BASESZ);
    unsigned long bufsz = BUF_BASESZ;
    if ((stat_file = fopen(CPU_STATFILE, "r")))
    {
        if (fgets(buf, bufsz, stat_file) == NULL)
        {
            return false;
        }
        // only the first four fields are relevant: others are just ignored
        // if the values are not read properly they still can be obtained from the sum of cores
        if (sscanf(buf, "cpu %lu %lu %lu %lu %*[^\n]\n", &usr, &usr_lowprio, &sys, &idle) == 4)
        {
            // calculate the output values based on the reads
            total = usr + usr_lowprio + sys + idle;
            // calculate the new values using deltas
            // USER_HZ just scales ticks to the original value. It only matters in raw mode
            float fact = USER_HZ / (float)(total - cpudata->total.prev_total);
            cpudata->total.perc_usr = (usr - cpudata->total.prev_usr) * fact;
            cpudata->total.perc_usr_nice = (usr_lowprio - cpudata->total.prev_usr_nice) * fact;
            cpudata->total.perc_sys = (sys - cpudata->total.prev_sys) * fact;
            cpudata->total.perc_idle = (idle - cpudata->total.prev_idle) * fact;
            // update the previous values with the current reads
            cpudata->total.prev_usr = usr;
            cpudata->total.prev_usr_nice = usr_lowprio;
            cpudata->total.prev_sys = sys;
            cpudata->total.prev_idle = idle;
            cpudata->total.prev_total = total;

            // scan all cores cpu[0,1, ...]
            for (int i = 0; i < cpudata->num_cores; i++)
            {
                if (fgets(buf, bufsz, stat_file) == NULL)
                {
                    return false;
                }
                sscanf(buf, "cpu%*d %lu %lu %lu %lu %*[^\n]", &usr, &usr_lowprio, &sys, &idle);

                // calculate the output values based on the reads
                total = usr + usr_lowprio + sys + idle;
                // calculate the new values using deltas
                float fact = USER_HZ / (float)(total - cpudata->percore[i].prev_total);
                cpudata->percore[i].perc_usr = (usr - cpudata->percore[i].prev_usr) * fact;
                cpudata->percore[i].perc_usr_nice = (usr_lowprio - cpudata->percore[i].prev_usr_nice) * fact;
                cpudata->percore[i].perc_sys = (sys - cpudata->percore[i].prev_sys) * fact;
                cpudata->percore[i].perc_idle = (idle - cpudata->percore[i].prev_idle) * fact;
                // update the previous values with the current reads
                cpudata->percore[i].prev_usr = usr;
                cpudata->percore[i].prev_usr_nice = usr_lowprio;
                cpudata->percore[i].prev_sys = sys;
                cpudata->percore[i].prev_idle = idle;
                cpudata->percore[i].prev_total = total;
            }
        }
        fclose(stat_file);
    }
    return (stat_file && buf ? true : false);
}

bool get_cpu_model(char **model, int *cores)
{
    FILE *cpuinfo = NULL;
    char buf[BUF_BASESZ];
    char mod[BUF_BASESZ];
    int found_core = -1;
    if ((cpuinfo = fopen(CPU_MODELFILE, "r")))
    {
        while (fgets(buf, BUF_BASESZ, cpuinfo))
        {
            if (sscanf(buf, "model name\t: %[^\n]\n", mod) == 1)
            {
                *model = strdup(mod);
            }
            sscanf(buf, "processor\t: %d", &found_core);
        }
        if (found_core == -1)
        {
            if (*model)
                free(model);
            return false;
        }
        *cores = found_core + 1; // cores are numbered starting from 0, hence the increment

        fclose(cpuinfo);
    }
    return (cpuinfo ? true : false);
}
