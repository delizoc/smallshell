# smallshell
Title: smallsh (Portfolio)

Description: smallsh program. Create shell in C. Program will prompt for running commands, handle blank lines
provide expansion for the variable $$, executre 3 commands (exit, cd and status), execute other commands by creating new processes
using function from the exec family of functions, support input and output redirection, support running commands in foreground and background
implement custom handlers for 2 signals, SIGNIT and SIGSTP.

To compile and run:
1. Within terminal type "make [enter]"
  This will run the "p3testscript" and output the rusults to file "mytestresults"
2. To compile and run without test script run "gcc -std=gnu99 -g -Wall -o smallsh smallsh.c"
3. Within the terminal type "smallsh [enter]"
4. You are now able to run commands such as "ls", "exit", "cd", "status" and execute other commands by creating new processes
using function from the exec family of functions, support input and output redirection, support running commands in foreground and background using ^c and ^z.
 5. To see all program requirements see [requirments.md](/requirements.md).
