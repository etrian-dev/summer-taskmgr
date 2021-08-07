#ifndef CPU_INFO_INCLUDED
#define CPU_INFO_INCLUDED

#include <glib.h>

#define CPU_STATFILE "/proc/stat"
#define BUF_BASESZ 256

// gets statistics about the cpu usage
gboolean get_cpu_info(
    gushort *cpu_count,
    gulong *user_time, gulong *user_time_prev,
    gulong *sys_time, gulong *sys_time_prev,
    gulong *tot_time, gulong *tot_time_prev);

// utility function to obtain the cpu usage percentage for the value passes as the first param
// the formula is (curr - prev) / (tot_current - tot_prev) iff curr > prev, 0 otherwise
gfloat calc_percentage(gulong current, gulong prev, gulong tot_current, gulong tot_prev);

#endif
