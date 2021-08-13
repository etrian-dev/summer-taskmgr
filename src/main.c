#include <time.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include <glib.h>
#include <pthread.h>

#include <ncurses.h>
#include <menu.h> // for menus

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"

#include "update_threads.h"

#include "windows.h"

WINDOW *memwin = NULL;
WINDOW *cpuwin = NULL;
WINDOW *procwin = NULL;

Mem_data_t *mem_stats = NULL;
CPU_data_t *cpu_stats = NULL;
TaskList *tasks = NULL;

void alarm_handler(int sig) {
    mem_window_update(memwin, mem_stats);
    cpu_window_update(cpuwin, cpu_stats);
    proc_window_update(procwin, tasks);
}

void stop_timer(timer_t timerid, struct itimerspec *oldval) {
    // disarm (stop) the timer that emits SIGALRM and pass to the caller its specification to be able to restart it
    struct itimerspec stop_timer_spec;
    // zeroed to have the field it_value zeroed, so that the timer is stopped
    memset(&stop_timer_spec, 0, sizeof(stop_timer_spec));
    timer_settime(timerid, 0, &stop_timer_spec, oldval); // save the interval
}

void print_menu(void) {
    erase();
    // defines menu items and their descriptions
    const int nitems = 6;
    char **menu = malloc(nitems * sizeof(char*));
    menu[0] = "q: Quit the program";
    menu[1] = "h: Help screen";
    menu[2] = "s: Change the process list sorting mode";
    menu[3] = "i: Freeze the state of the screen";
    menu[4] = "f: Find a pattern in the process list";
    menu[5] = "m: Hide the menu";

    int yoff = 1;
    int xoff = 1;
    for(int i = 0; i < nitems; i++) {
        mvprintw(yoff++, xoff, "- %s", menu[i]);
    }
    refresh();
}

void print_submenu(void) {
    erase();
    // submenu used to choose the process sorting mode
    char **sorting_modes = NULL;
    const int nmodes = 3;
    sorting_modes = malloc(nmodes * sizeof(char*));
    sorting_modes[0] = "cmd: Sorts processes using lexicographical order of their command line";
    sorting_modes[1] = "PID (incr): Sorts processes using increasing ordering on their PID";
    sorting_modes[2] = "PID (decr): Sorts processes using decreasing ordering on their PID";

    int yoff = 1;
    int xoff = 1;
    for(int i = 0; i < nmodes; i++) {
        mvprintw(yoff++, xoff, "- [%d] %s", i, sorting_modes[i]);
    }
    refresh();
}

int main(int argc, char **argv) {


    // create a timer that generates SIGALRM each interval
    // this timer is used to periodically refresh the ncurses windows displaying data
    timer_t alarm;
    timer_create(CLOCK_REALTIME, NULL, &alarm);

    // define a signal handler for SIGALRM
    struct sigaction handle_alarm;
    memset(&handle_alarm, 0, sizeof(handle_alarm));
    handle_alarm.sa_handler = alarm_handler;
    // to restart blocking functions interrupted by the signal without throwing EINTR
    handle_alarm.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &handle_alarm, NULL);

    // init ncurses
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();

    // create three indipendent windows
    memwin = newwin(LINES/4, COLS, 0, 0);
    cpuwin = newwin(LINES/4, COLS, LINES/4, 0);
    procwin = newwin(LINES/2, COLS, LINES/2, 0);

    // Memory
    mem_stats = calloc(1, sizeof(Mem_data_t));
    pthread_mutex_init(&mem_stats->mux_memdata, NULL);
    pthread_cond_init(&mem_stats->cond_updating, NULL);
    // CPU
    cpu_stats = calloc(1, sizeof(CPU_data_t));
    // sets the number of cores and the model of the CPU just once
    get_cpu_model(&(cpu_stats->model), &(cpu_stats->num_cores));
    pthread_mutex_init(&cpu_stats->mux_memdata, NULL);
    pthread_cond_init(&cpu_stats->cond_updating, NULL);

    // Processes
    tasks = calloc(1, sizeof(TaskList));
    tasks->ps = g_array_new(FALSE, FALSE, sizeof(Task));
    g_array_set_clear_func(tasks->ps, clear_task);
    // default process sorting criteria:lexicographical order of command lines
    tasks->sortfun = cmp_commands;
    pthread_mutex_init(&tasks->mux_memdata, NULL);
    pthread_cond_init(&tasks->cond_updating, NULL);

    pthread_t update_th[3];
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    // create them detached, so I can avoid joining them
    pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    pthread_create(&update_th[0], &attrs, update_mem, mem_stats);
    pthread_create(&update_th[1], &attrs, update_cpu, cpu_stats);
    pthread_create(&update_th[2], &attrs, update_proc, tasks);

    // declare the timer interval and starting offset
    struct timespec interval; // sends SIGALRM every second
    interval.tv_sec = 1;
    interval.tv_nsec = 0;
    struct timespec start_delay; // starts after two seconds
    start_delay.tv_sec = 2;
    start_delay.tv_nsec = 0;
    // this structure holds the timer interval and start delay
    struct itimerspec spec;
    spec.it_interval = interval;
    spec.it_value = start_delay;
    // arm (start) the timer using the structs defined above
    timer_settime(alarm, 0, &spec, NULL);

    gboolean run = TRUE;
    gboolean menu_shown = FALSE;
    while(run == TRUE) {
        if(menu_shown == TRUE) {
            print_menu();
        }
        int c = getch();
        switch(c) {
            case 'q':
                run = FALSE;
                break;
            case 'h':
                // unimplemented
                break;
            case 's': {
                // print the submenu and change the soring mode
                print_submenu();
                char s = getch();
                if(s == '0') {
                    switch_sortmode(tasks, cmp_commands);
                }
                if(s == '1') {
                    switch_sortmode(tasks, cmp_pid_incr);
                }
                if(s == '2') {
                    switch_sortmode(tasks, cmp_pid_decr);
                }
                break;
            }
            case 'f':
                // uniplemented
                break;
            case 'i':
                // unimplemented
                break;
            case 'm': // toggles menu visibility
                menu_shown = (menu_shown == TRUE ? FALSE : TRUE);
                // stop the timer if the menu will be shown
                if(menu_shown == TRUE) {
                    stop_timer(alarm, &spec);
                }
                // otherwise restart it because the menu will be hidden
                else {
                    timer_settime(alarm, 0, &spec, NULL);
                }
        }
    }
    delwin(memwin);
    delwin(cpuwin);
    delwin(procwin);
    endwin();

    return 0;
}

