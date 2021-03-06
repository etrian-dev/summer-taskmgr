#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <glib.h>
#include <pthread.h>

#include <ncurses.h>
#include <jansson.h> // to parse JSON

#include "mem_info.h"
#include "cpu_info.h"
#include "process_info.h"
#include "update_threads.h"
#include "windows.h"

#include "main.h"

/**
 * \brief The program's main function
 */
int main(int argc, char **argv)
{
    // loads menus descriptions from the json file menus.json
    json_error_t err;
    json_t *menus_descr = json_load_file(JSON_MENUFILE, 0, &err);
    if (!menus_descr)
    {
        fprintf(stderr, "JSON loading from %s failed: %s\n", JSON_MENUFILE, err.text);
        return 1;
    }
    // get the main menu and sort menu objects
    json_t *main_menu = json_object_get(menus_descr, "main_menu");
    json_t *sort_menu = json_object_get(menus_descr, "sort_menu");
    if (main_menu == NULL || sort_menu == NULL)
    {
        json_decref(menus_descr);
        fprintf(stderr, "Error in getting the menu object\n");
        return 1;
    }
    // main menu items, descriptions and keybindings
    size_t mainmenu_sz = json_object_size(main_menu);
    if (mainmenu_sz == 0)
    {
        fprintf(stderr, "Not a json object supplied");
        return 1;
    }
    char **menuitems = malloc(mainmenu_sz * sizeof(char *));
    char **menudescr = malloc(mainmenu_sz * sizeof(char *));
    int *keybinds_main = malloc(mainmenu_sz * sizeof(int));
    // sorting menu items, descriptions and keybindings
    size_t sortmenu_sz = json_object_size(sort_menu);
    if (mainmenu_sz == 0)
    {
        fprintf(stderr, "Not a json object supplied");
        return 1;
    }
    char **sortmodes_items = malloc(sortmenu_sz * sizeof(char *));
    char **sortmodes_descr = malloc(sortmenu_sz * sizeof(char *));
    int *keybinds_sort = malloc(sortmenu_sz * sizeof(int));
    // get the contents of the main_menu and sort_menu objects to fill the arrays alloc'd above
    const char *item;
    json_t *val;
    int i = 0;
    json_object_foreach(main_menu, item, val)
    {
        json_t *keybind = json_array_get(val, 0); // item keybind
        json_t *descr = json_array_get(val, 1);   // item description
        menuitems[i] = strdup(item);
        menudescr[i] = strdup(json_string_value(descr));
        keybinds_main[i] = (json_string_value(keybind))[0]; // the keybind is just a character, so it can be safely copied
        i++;
    }
    i = 0;
    json_object_foreach(sort_menu, item, val)
    {
        json_t *descr = json_array_get(val, 1); // item description
        sortmodes_items[i] = strdup(item);
        sortmodes_descr[i] = strdup(json_string_value(descr));
        keybinds_sort[i] = i + '0'; // the keybind is just the index in the array as a character
        i++;
    }
    // initialize the array of function pointers with all the modes defined in process_info.h
    int (**sorting_modes)(const void *, const void *) = malloc(sortmenu_sz * sizeof(*sorting_modes));
    sorting_modes[0] = cmp_commands;
    sorting_modes[1] = cmp_usernames;
    sorting_modes[2] = cmp_pid_incr;
    sorting_modes[3] = cmp_pid_decr;
    sorting_modes[4] = cmp_nthreads_inc;
    sorting_modes[5] = cmp_nthreads_decr;

    // creates a timer that generates SIGALRM each interval
    // this timer is used to periodically refresh the windows displaying data
    timer_t alarm;
    timer_create(CLOCK_REALTIME, NULL, &alarm);

    // all signals to this thread are blocked (inherited by the threads created by this one)
    sigset_t masked_sigs;
    sigfillset(&masked_sigs);
    pthread_sigmask(SIG_BLOCK, &masked_sigs, NULL);

    struct taskmgr_data_t shared_data;
    // scaling is activated by default
    shared_data.rawdata = 1;

    // Initialize the memory data structure
    shared_data.mem_stats = calloc(1, sizeof(Mem_data_t));
    pthread_mutex_init(&(shared_data.mem_stats->mux_memdata), NULL);
    pthread_cond_init(&(shared_data.mem_stats->cond_updating), NULL);

    // Does the same for CPU
    shared_data.cpu_stats = calloc(1, sizeof(CPU_data_t));
    // sets the number of cores and the model of the CPU just once at startup, since that's unlikely to change
    get_cpu_model(&(shared_data.cpu_stats->model), &(shared_data.cpu_stats->num_cores));
    // initialize the per-core statistics array
    shared_data.cpu_stats->percore = calloc(shared_data.cpu_stats->num_cores, sizeof(struct core_data_t));
    pthread_mutex_init(&(shared_data.cpu_stats->mux_memdata), NULL);
    pthread_cond_init(&(shared_data.cpu_stats->cond_updating), NULL);

    // And for processes
    shared_data.tasks = calloc(1, sizeof(TaskList));
    shared_data.tasks->ps = g_array_new(false, false, sizeof(Task));
    g_array_set_clear_func(shared_data.tasks->ps, clear_task);
    shared_data.tasks->num_ps = 0;
    shared_data.tasks->num_threads = 0;
    // default process sorting criteria: lexicographical order of command lines
    shared_data.tasks->sortfun = cmp_commands;
    shared_data.tasks->cursor_start = 0; // the cursor starts at the first process
    pthread_mutex_init(&shared_data.tasks->mux_memdata, NULL);
    pthread_cond_init(&shared_data.tasks->cond_updating, NULL);

    // creates the threads that handle data update
    pthread_t update_th[4];
    pthread_create(&update_th[0], NULL, signal_thread, &shared_data);
    pthread_create(&update_th[1], NULL, update_mem, shared_data.mem_stats);
    pthread_create(&update_th[2], NULL, update_cpu, shared_data.cpu_stats);
    // this thread needs data from the memory and the CPU
    pthread_create(&update_th[3], NULL, update_proc, &shared_data);

    // inititalize ncurses with some useful additions
    initscr();
    if (has_colors() == true)
    {
        start_color(); // start using colors
    }
    cbreak();
    keypad(stdscr, true);
    noecho();
    // define a green on black color pair
    short green_on_black = 1;
    init_pair(green_on_black, COLOR_GREEN, COLOR_BLACK);

    // create three indipendent windows to handle memory, cpu and process display
    // The first two occupy each a quarter of the screen, while the third fills the rest
    shared_data.memwin = newwin(LINES / 4, COLS, 0, 0);
    shared_data.cpuwin = newwin(LINES / 4, COLS, LINES / 4, 0);
    shared_data.procwin = newwin(LINES / 2, COLS, LINES / 2 - 1, 0);

    // declare the timer interval and starting offset
    struct timespec interval; // sends SIGALRM every second
    interval.tv_sec = 1;
    interval.tv_nsec = 0;
    struct timespec start_delay; // starts after two seconds
    start_delay.tv_sec = 2;
    start_delay.tv_nsec = 0;
    struct itimerspec spec;
    spec.it_interval = interval;
    spec.it_value = start_delay;
    // arm (start) the timer using the time quantities defined above
    timer_settime(alarm, 0, &spec, NULL);

    // Main application loop: it receives character input directly from the user, but it's
    // interrupted by SIGALRMs each interval
    bool run = true;
    bool menu_shown = false;   // true if the menu below must be printed (updates to data are not shown to the user)
    bool searching = false;    // flag set iff a searching is in progress
    bool killing = false;      // flag set iff the user is typing a PID to kill
    int key = 0, last_key = 0; // remembers the last key pressed
    // This is needed to have a shorter timer interval only when the user is scrolling
    // so that the user does not notice a stuttering when holding down the arrow key
    while (run == true)
    {
        if (menu_shown == true)
        {
            print_menu(keybinds_main, menuitems, menudescr, mainmenu_sz);
        }
        // gets user input (be aware that this works whether the menu is shown or not)
        key = getch();
        switch (key)
        {
        case 'q':
            run = false;
            break;
        case 'h':
            // unimplemented
            break;
        case 's':
        {
            // print the submenu and change the sorting mode
            print_menu(keybinds_sort, sortmodes_items, sortmodes_descr, sortmenu_sz);
            char s = getch();
            if (s >= '0' && s <= '0' + sortmenu_sz - 1)
            {
                switch_sortmode(shared_data.tasks, sorting_modes[s - '0']);
            }
            break;
        }
        case 'i':
            // unimplemented
            break;
        case 'f':
        {
            if (searching == true)
            {
                // 'f' was pressed to quit the search view: reset all highlighted processes to normal
                for (int i = 0; i < shared_data.tasks->num_ps; i++)
                {
                    Task *t = &g_array_index(shared_data.tasks->ps, Task, i);
                    if (t->highlight == true)
                    {
                        t->highlight = false;
                    }
                }
                searching = false;
                // clears the search line (LINES - 1)
                wmove(stdscr, LINES - 1, 0);
                wclrtoeol(stdscr);
            }
            else
            {
                // search for a pattern in the process list
                find_pattern(shared_data.tasks);
                // hide the menu
                menu_shown = false;
                // set the searching flag
                searching = true;
                // restart the timer
                timer_settime(alarm, 0, &spec, NULL);
                // refresh the window immediately: do not wait for a time tick
                // to show results
                proc_window_update(shared_data.procwin, shared_data.tasks);
            }
            break;
        }
        case 'r': // show/hide raw values
            if (shared_data.rawdata == 0)
            {
                shared_data.rawdata = 1;
            }
            else if (shared_data.rawdata == 1)
            {
                shared_data.rawdata = 0;
            }
            break;
        // TODO: experimental
        case 'k': // kill a process
            if (killing == true)
            {
                killing = false;
                // kill mode is being exited
                wmove(stdscr, LINES - 1, 0);
                wclrtoeol(stdscr);
            }
            else
            {
                // read a PID and try to kill the process
                kill_process(shared_data.tasks);
                // hide the menu
                menu_shown = false;
                // set the killing flag
                killing = true;
                // restart the timer
                timer_settime(alarm, 0, &spec, NULL);
                // update the window immediately (otherwise it will refresh at the next timer tick)
                proc_window_update(shared_data.procwin, shared_data.tasks);
            }
            break;
        // TODO: experimental
        case 'e':
        {
            // create a new process using fork/exec*()
            char *exe = read_pattern(stdscr, LINES - 1, 1, "(exec): ");
            pid_t newp = fork();
            if (newp == 0)
            {
                // exe must be in PATH in order for the call to succeed
                // p suffix to search in PATH for the binary
                if (execlp(exe, exe, NULL) == -1)
                {
                    wmove(stdscr, LINES - 1, 1);
                    wclrtoeol(stdscr);
                    attron(COLOR_PAIR(green_on_black));
                    printw("Cannot exec %s: %S", exe, strerror(errno));
                    attroff(COLOR_PAIR(green_on_black));

                    exit(1);
                }
            }
            break;
        }
        case 'm': // toggles menu visibility
            menu_shown = (menu_shown == true ? false : true);
            // stop the timer if the menu will be shown
            if (menu_shown == true)
            {
                stop_timer(alarm, &spec);
            }
            // otherwise restart it because the menu will be hidden
            else
            {
                timer_settime(alarm, 0, &spec, NULL);
            }
        // handling of the process cursor movement (to simulate a scrollable window)
        case KEY_DOWN:
            // if a key is pressed
            if (shared_data.tasks->cursor_start < shared_data.tasks->num_ps - 1)
                shared_data.tasks->cursor_start++;
            wrefresh(shared_data.procwin);
            break;
        case KEY_UP:
            if (shared_data.tasks->cursor_start > 0)
                shared_data.tasks->cursor_start--;
            wrefresh(shared_data.procwin);
            break;
        case KEY_END:
            shared_data.tasks->cursor_start = shared_data.tasks->num_ps - 1;
            wrefresh(shared_data.procwin);
            break;
        case KEY_HOME:
            shared_data.tasks->cursor_start = 0;
            wrefresh(shared_data.procwin);
            break;
        case KEY_NPAGE:
        {
            int prows, pcols;
            getmaxyx(shared_data.procwin, prows, pcols);
            shared_data.tasks->cursor_start += prows - 4;
            if (shared_data.tasks->cursor_start > shared_data.tasks->num_ps)
            {
                shared_data.tasks->cursor_start = shared_data.tasks->num_ps - 1;
            }
            wrefresh(shared_data.procwin);
            break;
        }
        case KEY_PPAGE:
        {
            int prows, pcols;
            getmaxyx(shared_data.procwin, prows, pcols);
            shared_data.tasks->cursor_start -= prows - 4;
            if (shared_data.tasks->cursor_start < 0)
            {
                shared_data.tasks->cursor_start = 0;
            }
            wrefresh(shared_data.procwin);
            break;
        }
        }

        // if both the last pressed key and the current are scrolling keys (one of those listed above)
        // shorten the timer tick and restart it
        if ((key == KEY_DOWN || key == KEY_UP || key == KEY_NPAGE || key == KEY_PPAGE || key == KEY_END || key == KEY_HOME) && (last_key == KEY_DOWN || last_key == KEY_UP || last_key == KEY_NPAGE || last_key == KEY_PPAGE || last_key == KEY_END || last_key == KEY_HOME))
        {
            struct itimerspec short_tick;
            short_tick.it_interval.tv_sec = 0;
            short_tick.it_interval.tv_nsec = 250000000;
            short_tick.it_value.tv_sec = 0;
            short_tick.it_value.tv_nsec = 100000;
            timer_settime(alarm, 0, &short_tick, &spec);
        }
        // if the current key is not a scrolling key and the last key was
        // then reset the timer to the normal tick
        if ((key != KEY_DOWN && key == KEY_UP && key != KEY_NPAGE && key != KEY_PPAGE && key != KEY_END && key != KEY_HOME) && (last_key == KEY_DOWN || last_key == KEY_UP || last_key == KEY_NPAGE || last_key == KEY_PPAGE || last_key == KEY_END || last_key == KEY_HOME))
        {
            timer_settime(alarm, 0, &spec, NULL);
        }

        // update the last key pressed
        last_key = key;
    }

    // free menu items and descriptions
    for (i = 0; i < mainmenu_sz; i++)
    {
        free(menuitems[i]);
        free(menudescr[i]);
    }
    free(menuitems);
    free(menudescr);
    for (i = 0; i < sortmenu_sz; i++)
    {
        free(sortmodes_items[i]);
        free(sortmodes_descr[i]);
    }
    free(sortmodes_items);
    free(sortmodes_descr);
    free(keybinds_main);
    free(keybinds_sort);
    free(menus_descr); // the json_t object used to load the menu
    // frees the sorting modes array
    free(sorting_modes);
    // deletes the alarm timer
    timer_delete(alarm);
    // frees the memory, cpu and process data structures
    free(shared_data.mem_stats);
    if (shared_data.cpu_stats->model)
        free(shared_data.cpu_stats->model);
    free(shared_data.cpu_stats->percore);
    free(shared_data.cpu_stats);
    if (shared_data.tasks->ps)
        g_array_free(shared_data.tasks->ps, true); // frees data stored inside as well
    shared_data.tasks->sortfun = NULL;
    free(shared_data.tasks);
    // deletes all the WINDOWs and end ncurses mode
    delwin(shared_data.memwin);
    delwin(shared_data.cpuwin);
    delwin(shared_data.procwin);
    endwin();

    return 0;
}
