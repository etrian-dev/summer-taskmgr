#include "process_info.h"

#include <string.h>

void switch_sortmode(TaskList *tasks, int (*newmode)(const void *, const void *)) {
    tasks->sortfun = newmode;
}

// default sorting function for processes in the process array
// returns -1 iff process a's cmdline (as read from /proc/[a_pid]/cmdline) is
// lexicographically less than b's or b's is NULL. It returns 0 if both cmdlines are NULL
// and 1 if b's is less than a's or a's cmdline is NULL
int cmp_commands(const void *a, const void *b) {
    Task *ta = (Task*)a;
    Task *tb = (Task*)b;
    if(!(ta->command || tb->command)) {
        return 0;
    }
    if(!ta->command && tb->command) {
        return 1;
    }
    if(ta->command && !tb->command) {
        return -1;
    }
    // like strcmp, but ignores case
    return strcasecmp(ta->command, tb->command);
}
// lexicographical username sorting (NULL usernames last)
int cmp_usernames(const void *a, const void *b) {
    Task *ta = (Task*)a;
    Task *tb = (Task*)b;
    if(!(ta->username || tb->username)) {
        return 0;
    }
    if(!ta->username && tb->username) {
        return 1;
    }
    if(ta->username && !tb->username) {
        return -1;
    }
    // all usernames are lowercase [verify?], so strcasecmp isn't needed
    return strcmp(ta->username, tb->username);
}
// pid increasing sorting
int cmp_pid_incr(const void *a, const void *b) {
    return ((Task*)b)->pid - ((Task*)a)->pid;
}
// pid decreasing sorting
int cmp_pid_decr(const void *a, const void *b) {
    return ((Task*)a)->pid - ((Task*)b)->pid;
}
// increasing thread count
int cmp_nthreads_inc(const void *a, const void *b) {
    return ((Task*)b)->num_threads - ((Task*)a)->num_threads;
}
// decreasing thread count
int cmp_nthreads_decr(const void *a, const void *b) {
    return ((Task*)a)->num_threads - ((Task*)b)->num_threads;
}
