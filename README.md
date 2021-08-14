# Summer task manager
A simple terminal-based task manager written in C, with a TUI interface written in ncurses  
*Prerequisites*:
- ncurses library
- glib
- pthreads library
- linux headers, such as `unistd.h`
## Compiling 
Just `make` and it should be fine, given the prerequisites above are satisfied. 
An executable named ./a.out will be produced in the base directory of the repository
## Execution
The task manager has a main screen containing memory meters, cpu usage statistics
and a process list. A simple menu (hidden at startup) allows the user to perform a few actions;
the menu is toggled (shown/hidden) by pressing the 'm' key.  
Actually the operations can be performed without needing to open the menu, but some (such as 
changing the sorting function won't work as expected (it opens the submenu, but when the timer fires SIGALRM 
the main window is refreshed, and hence the sumbenu is hidden under it. You can still set the sorting mode, though)
### Menu options
- q: Quit the program
- h: Shows the help screen (unimplemented)
- s: Change the process sorting mode (this opens a new submenu)
- i: Freeze the screen (unimplemented)
- f: Find a pattern in the process list (unimplemented)
- m: Hide the menu
