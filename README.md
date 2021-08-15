# Summer task manager
A simple terminal-based task manager written in C, with a TUI interface written in ncurses  
**Prerequisites**:
- ncurses library (with the menu addon -lmenu)
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
- f: Find a pattern in the process list (still under developement)
- m: Show/Hide the menu
### Usage
To access the menu type 'm'. You will be presented with the set of options described above. 
Finding patterns works properly (and it's probably more useful) without entering the menu. 
A search is performed by typing a pattern (**currently deletions are not handled**) up to a newline (sent by <Enter>). 
The pattern provided is searched as a substring of any process command line currently in the task list. 
Any process whose command line matches pattern is then highlighted until 'f' is pressed to exit the search mode. 
The number of matching processes is printed as well on the same line as the search prompt (the last on the terminal window).
No other search or refinement can currently be performed while a search is in progress. When the search mode is 
exited the line presenting it is cleared by wclrtoeol()
