/**
 * \file utilities.c
 * \brief Utility functions used by main.c
 */

#include "main.h"

/**
 * \brief Stops the given POSIX timer timerid and passes to the caller its past settings
 *
 * The POSIX timer timerid (created by timer_create) is disarmed (stopped) by setting
 * its it_value field as zeros. The old interval and starting delay is saved and passed
 * to the caller, so that it may restart the timer with the same settings
 * \param [in] timerid The ID of the timer to be stopped
 * \param [out] oldval The timer's settings before disarming it
 */
void stop_timer(timer_t timerid, struct itimerspec *oldval) {
    // disarm (stop) the timer that emits SIGALRM and pass to the caller its specification to be able to restart it
    struct itimerspec stop_timer_spec;
    // zeroed to have the field it_value zeroed, so that the timer is stopped
    memset(&stop_timer_spec, 0, sizeof(stop_timer_spec));
    timer_settime(timerid, 0, &stop_timer_spec, oldval); // save the interval
}
/**
 * \brief Prints a menu on the screen from an array of keybindings, items and descriptions
 *
 * The function takes arrays of items, descriptions and the relative keybind associated
 * with it using entries of the form:
 * 	[keybind] item: description
 * \param [in] keybinds An array of bindings associated with the corresponding item
 * \param [in] items An array of item names, representing an action the user can perform
 * \param [in] descr An array of descriptions of the corresponding item in items
 * \param [in] nitems The lenght of each of the input arrays (their lengths must be equal)
 * \return Returns 0 if the menu was printed successfully, -1 otherwise
 */
int print_menu(int *keybinds, char **items, char **descriptions, const int nitems) {
    int yoff = 1;
    erase();
    for(int i = 0; i < nitems && i < LINES; i++) {
        mvprintw(yoff++, 1, "[%c] %s: %s", (char)keybinds[i], items[i], descriptions[i]);
    }
    refresh();
    return 0;
}
