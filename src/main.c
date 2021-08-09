/*
 * main.c
 *
 * Copyright 2021 nicola vetrini <nicola@ubuntu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <ncurses.h>

#include "cpu_info.h"
#include "mem_info.h"
#include "process_info.h"

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
	return strcasecmp(ta->command, tb->command);
}

int main(int argc, char **argv)
{
	// init curses
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	int rows, cols;
	getmaxyx(stdscr, rows, cols);

	// refresh rate (approx)
	struct timespec delay;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;
	//delay.tv_nsec = 500000000; // 5 * 10^8 ns = 0.5s

	char progbar[102];
	progbar[0] = '[';
	progbar[101] = ']';
	memset(progbar + 1, '.', 100 * sizeof(char));
	char scale[102];
	memset(scale + 1, '_', 100 * sizeof(char));
	scale[0] = '|';
	scale[25] = '|';
	scale[50] = '|';
	scale[75] = '|';
	scale[101] = '|';

	int xoff = 1;
	int yoff = 1;
	int filter_ln = 0;

	/////////////////////////////////////////////////////////////////////////////////
	// CPU

	CPU_data_t cpu_usage;
	memset(&cpu_usage, 0, sizeof(CPU_data_t));
	int core_count;

	/////////////////////////////////////////////////////////////////////////////////

	// Memory
	/////////////////////////////////////////////////////////////////////////////////

	gulong total_mem = 0;
	gulong free_mem = 0;
	gulong avail_mem = 0;
	gulong buffer_cached = 0;
	gulong swp_tot = 0;
	gulong swp_free = 0;

	/////////////////////////////////////////////////////////////////////////////////

	// Tasks
	/////////////////////////////////////////////////////////////////////////////////

	// the array of structures representig processes
	GArray *ps = g_array_new(FALSE, FALSE, sizeof(Task));
	g_array_set_clear_func(ps, clear_task);

	long num_ps = 0;
	long num_threads = 0;

	/////////////////////////////////////////////////////////////////////////////////

	// create the input thread
	char *filter_str = NULL;

	while(1) {
		get_mem_info(&total_mem, &free_mem, &avail_mem, &buffer_cached, &swp_tot, &swp_free);
		get_cpu_info(&core_count, &cpu_usage);
		get_processes_info(ps, &num_ps, &num_threads);

		// sort the process array lexicographically by command name
		g_array_sort(ps, cmp_commands);

		// sets the percentage of memory not available for new processes (not necessarily in use)
		// as the
		gfloat percent = 100.0 - (avail_mem * 100.0)/(float)total_mem;
		gulong quot = floor(percent);
		memset(progbar + 1, '#', quot * sizeof(char));

		mvaddstr(yoff++, xoff, "MEMORY");
		mvprintw(yoff++, xoff, "%s (%.3f%% in use)", progbar, percent);
		mvaddstr(yoff++, xoff, scale);
		mvprintw(
			yoff++, xoff,
			"Total: %lu MiB\tAvailable: %lu MiB\tFree: %lu MiB\tBuff/Cached: %lu MiB",
			total_mem / 1024, avail_mem / 1024, free_mem / 1024, buffer_cached / 1024);

		// does the same thing, but for swap (resets the bar first)
		memset(progbar + 1, '.', quot * sizeof(char));
		percent = 100 - (swp_free * 100)/(float)swp_tot;
		memset(progbar + 1, '#', percent * sizeof(char));

		mvprintw(yoff++, xoff, "%s (%.3f%% in use)", progbar, percent);
		mvaddstr(yoff++, xoff, scale);
		mvprintw(yoff++, xoff, "Swap Total: %lu MiB\tSwap Free: %lu MiB", swp_tot / 1024, swp_free / 1024);

		mvprintw(yoff++, xoff, "CPU %s\tcores %d", "<placeholder>", core_count);
		mvprintw(yoff++, xoff,
			"CPU%% usr: %.3f\tnice: %.3f\tsys: %.3f\tidle: %.3f",
			cpu_usage.perc_usr, cpu_usage.perc_usr_nice, cpu_usage.perc_sys, cpu_usage.perc_idle);

		// print the list of processes (as long as they fit in the window
		filter_ln = yoff; // save the line where the process filter string should be typed
		yoff++;
		mvaddstr(yoff++, xoff, "PROCESSES");
		mvprintw(yoff++, xoff, "processes: %lu\tthreads: %lu", num_ps, num_threads);
		attron(A_STANDOUT|A_BOLD);
		mvprintw(yoff++, xoff,
			"%-10s %-10s %-5s %-5s %-10s %-10s %-10s",
			"PID", "PPID", "STATE", "NICE", "THREADS", "VSZ (kB)", "CMD");
		attroff(A_STANDOUT|A_BOLD);
		for(int i = 0; i + yoff < rows - 1; i++) {
			Task *t = &(g_array_index(ps, Task, i));
			mvprintw(i + yoff, xoff,
				"%-10ld %-10ld %-5c %-5ld %-10ld %-10ld %s",
				t->pid, t->ppid, t->state, t->nice, t->num_threads, t->virt_size_bytes / 1024, t->command);
		}

		move(filter_ln, xoff);
		if(filter_str) {
			attron(A_BOLD);
			mvaddstr(filter_ln, xoff, filter_str);
			attroff(A_BOLD);
		}

		// reset the array
		g_array_free(ps, TRUE);
		ps = g_array_new(FALSE, FALSE, sizeof(Task));
		g_array_set_clear_func(ps, clear_task);

		num_ps = 0;
		num_threads = 0;

		refresh();

		memset(progbar + 1, '.', percent * sizeof(char));
		yoff = 1;

		nanosleep(&delay, NULL);
	}
	endwin();
	return 0;
}

