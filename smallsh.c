/*
Author: Colette DeLizo
Date:29OCT2021
CS344 Assignment 3
Title: smallsh (Portfolio)
Description: smallsh program. Create shell in C. Program will prompt for running commands, handle blank lines
provide expansion for the variable $$, executre 3 commands (exit, cd and status), execute other commands by creating new processes
using function from the exec family of functions, support input and output redirection, support running commands in foreground and background
implement custom handlers for 2 signals, SIGNIT and SIGSTP.
*/
#include <stdio.h>
#include <sys/types.h> 
#include <unistd.h>	
#include <sys/wait.h>
//getpid(), fork(), wait() from module 4

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h> // signal handler
#include <fcntl.h> // close file descriptor on exec

#define MAXSIZE 2048	//command line input max length 2048 characters

int toggle = 1; 	 // if 1 we are still in background and can use & is 0 we are in foreground only can ignore &
int SPAWNCOUNT = 0;  //used to track where next spawnPid will be stored in spawnPid array, to be used to check if a background process has finished.

void userInput(char*[], int*, char[], char[], int);
void execCommand(char*[], int*, struct sigaction, int*, char[], char[], pid_t[]);
void printExitStatus(int);
void checkStatus(pid_t[], int*);
void stpHandler(int);


int main() {

	int pid = getpid();			//returns the process ID of the calling process
	bool flag = 1;
	int i;
	int exitStatus = 0;
	int background = 0;

	char inputFile[256] = "";
	char outputFile[256] = "";
	pid_t spawnPidArray[512];
	char* input[512];		//command from entered by user max 512 arguments
	for (i=0; i<512; i++) {	//create null input array for user arguments
		input[i] = NULL;
	}
	
	//code copied from CS344 moduel example: custom handler for SIGINT, ignores ^C
	struct sigaction SIGIGN_action = {0};
	SIGIGN_action.sa_handler = SIG_IGN;	
	sigfillset(&SIGIGN_action.sa_mask);
	sigaction(SIGINT, &SIGIGN_action, NULL);
	
	struct sigaction SIGSTP_action = {0};	
	SIGSTP_action.sa_handler = stpHandler;	//stpHandler toggles foreground and background ability by catching signal
	sigfillset(&SIGSTP_action.sa_mask);
	SIGSTP_action.sa_flags = SA_RESTART;	// SA-RESTART idea from https://edstem.org/us/courses/14269/discussion/800937
	sigaction(SIGTSTP, &SIGSTP_action, NULL);
	
	do {
		checkStatus(spawnPidArray, &exitStatus);		// before prompting for another command check to see if background processes are complete
		userInput(input, &background, inputFile, outputFile, pid); 	//command prompt and returns users input
				
		if (input[0][0] == '#' || input[0][0] == '\0') { //ignore blank linkes and comment lines begin with #character (to be ignored)
			continue;		
		}
		else if (strcmp(input[0], "exit") == 0) {	// execute exit command
			flag = 0;	// exit while loop, flag changed to false
		}
		else if (strcmp(input[0], "cd") == 0) {		//execute cd command
			if (input[1]) {							//execute chdir to directory specified in input[1]
				if (chdir(input[1]) == -1) {		//if directory specified does not exist, chdir returns -1
					printf("Directory not found.\n");
					fflush(stdout);					//flush output buffer and move buffered data to console
				}
			} else {								//if no directory is specified 
				chdir(getenv("HOME"));				//getenv searches environemnt list to find environment with name "HOME".
			}
		}
		else if (strcmp(input[0], "status") == 0) {	//execute status command
			printExitStatus(exitStatus);
			fflush(stdout);			//prints either exit status or terminating signal of last foreground process ran by the shell
		}
		else {			//execute other commands if input is not one of the 3 built in commands.
			execCommand(input, &exitStatus, SIGIGN_action, &background, inputFile, outputFile, spawnPidArray);
			fflush(stdout);
		}
		for (i=0; input[i]; i++) {		//clear input array to NULL
			input[i] = NULL;
		}
		background = 0;					//background, inputfile and output file reset
		inputFile[0] = '\0';
		outputFile[0] = '\0';

	} while (flag);		//while flag is true (only false with "exit" command)
	
	return 0;
}
//handler for SIGTSTP, changes toggle and prints message to tell user what mode ther are in.
void stpHandler(int signo) {
	if (toggle){
		write(1, "\nEntering foreground-only mode (& is now ignored)\n", 51);
		write(1,": ",2);	//print command line prompt
		toggle = 0;
		fflush(stdout);
	}
	else{
		write (1, "\nExiting foreground-only mode\n", 31);
		write(1,": ",2);	//print command line prompt
		toggle = 1;
		fflush(stdout);
	}

}
/*For user input we want to first replace \n with \0 and then make sure the line is not blank. After we want to create tokens of each command word to figure out what 
the user is asking. Once we figure this out we return to main function to execute either the 3 built in commands or exec()*/
void userInput(char* arr[], int* background, char inputName[], char outputName[], int pid) {
	char input[MAXSIZE];
	int i;
	int j;
	printf(": ");			//print command line ":"
	fflush(stdout);			//flushes stream
	fgets(input, MAXSIZE, stdin);		//reads line from terminal window, stores into buffer pointed to by stdin file stream
	
	int end = 0;	
	for (i=0; end == 0 && i<MAXSIZE; i++) {		//loop through input array to swap end of line character to null terminator
		if (input[i] == '\n') {					//go to the end of the line
			input[i] = '\0';					//replace \n with \0 NULL terminator for end of string
			end = 1;							//update flag end to exit loop
		}
	}
	if (strcmp(input, "") == 0) {		//if blank line
		arr[0] = strdup("");			//save pointer to blank string arr[0] and return to main()
		return;
	}
	/*
	strtok code with space adapted from :https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm
	to get next token code adapted from :https://stackoverflow.com/questions/23456374/why-do-we-use-null-in-strtok/23456549
					see comment "subsequent call is specified by passing the NULL as input to strtok( NULL , const char *delimiters )."
	Date: 30OCT2021
	*/

	const char space[2] = " ";	// delim space constant		 
	char *token = strtok(input, space);		//parse input into tokens

	for (i=0; token; i++) {
		if (strcmp(token, "&") == 0) {		// if & token exists this is background function
			*background = 1;				// set background flag to true
		}
		else if (strcmp(token, "<") == 0) {		//if < token, then input file
			token = strtok(NULL, space);		// go to next token
			strcpy(inputName, token);			// copy token into inputName
		}
		else if (strcmp(token, ">") == 0) {		//if > token, then output file
			token = strtok(NULL, space);		//go to next token
			strcpy(outputName, token);			// copy token into outputName
		}
		else {
			arr[i] = strdup(token); //add pointer to arr for token
			for (j=0; arr[i][j]; j++) {				// read pointer in arr for "$$"" for varibale expansion
				if (arr[i][j] == '$' && arr[i][j+1] == '$') {
					arr[i][j] = '\0';		
					snprintf(arr[i], 256, "%s%d", arr[i], pid); //print pid to expand $$
				}
			}
		}
		token = strtok(NULL, space);		//go to next token
	}
}
//excCommand will execute the command stored in arr
void execCommand(char* arr[], int* childStatus, struct sigaction sa, int* background, char inputName[], char outputName[], pid_t spawnPidArray[]) {
	int sourceFD;
	int result;
	int targetFD;

	pid_t spawnPid = fork();		//child spawn process adapted from exloration: process and I/O
	
	switch (spawnPid) {
		
		case -1:	;
			perror("fork() failed\n");
			exit(1);
			break;
		case 0:	;
			sa.sa_handler = SIG_DFL;		//foreground process wont ignore ^c, restore defualt signals
			sigaction(SIGINT, &sa, NULL);
			//file handling using redirection and dup2 code taken from exploration example: redirecting both stdin stdout
			if (strcmp(inputName,"")) {					//file open code adapted from Exploration: files
				sourceFD = open(inputName, O_RDONLY);		//open file with given name for reading only.
				if (sourceFD == -1) {
					printf("cannot open %s for input\n", inputName);
					exit(1);
				}
				result = dup2(sourceFD, 0);				//Redirect stdin to source file
				if (result == -1) {
					perror("source dup2()");
					exit(2);
				}
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);		//close file descriptor on exec from exploration: process and I/O
			}
			if (strcmp(outputName,"")) {
				targetFD = open(outputName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (targetFD == -1) {
					perror("target open()");
					exit(1);
				}
				result = dup2(targetFD, 1);		//redirect stdout to traget file
				if (result == -1) {
					perror("target dup2()");
					exit(2);
				}
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}
			//now use exec to execute command. 
			if (execvp(arr[0], (char* const*)arr)) {			//use execvp with filename as argument and uses PATH env for an executable filename, array passed as argument and is provided as an array of strings
				printf("%s: no such file or directory\n", arr[0]);
				fflush(stdout);
				exit(2);
			}
			break;
		
		default:	;
			// Execute a process in the background ONLY if toggle
			if (*background && toggle) {
				waitpid(spawnPid, childStatus, WNOHANG);
				printf("background pid is %d\n", spawnPid);
				fflush(stdout);
				// Stores the background process id in the global array for the pid_t's and increment the arrays placement so it doesnt override any other background process id's	
				spawnPidArray[SPAWNCOUNT] = spawnPid;		//idea to store pid in array adapted from ed discussion		
				SPAWNCOUNT++;
			}
			// Otherwise execute it like normal
			else {
				waitpid(spawnPid, childStatus, 0);
				fflush(stdin);
				if (WTERMSIG(*childStatus) == 2){		//if ^c is caught print terminated by signal 2 (couldnt figure out a better way to do this)
					printExitStatus(*childStatus);
					fflush(stdout);
				}
			}
	}
}
//prints exit status prior to allowing user to enter another command.
void checkStatus(pid_t spawnPidArray[], int* childStatus){		//checks to see if background processes are finished
	int i;
	int spawnPid;
	for (i = 0; i <= SPAWNCOUNT; i++){						//check each pid stored in array, if its complete print to terminal
		spawnPid = spawnPidArray[i];
		while ((spawnPid = waitpid(-1, childStatus, WNOHANG)) > 0) {	//when process is finished print exit status of background process
			printf("background pid %d is done: ", spawnPid);
			fflush(stdout);
			printExitStatus(*childStatus);
			fflush(stdout);
		}
	}	
}
/*
WIFEXITED, WEXITSTATUS and WTERMSIG concepts and code from module 4
*/
void printExitStatus(int childStatus) {
	if (WIFEXITED(childStatus)) {		//WIFEXITED macro returns true if terminated normally only use WEXITEDSTATUS if true else returns garbage
		printf("exit value %d\n", WEXITSTATUS(childStatus));	//WEXITEDSTATUS macro returns the exit status of the child
		fflush(stdout);
	} else {								// only use WTERMSIG if WIFEXITED returns false
		printf("terminated by signal %d\n", WTERMSIG(childStatus));
		fflush(stdout);
	}
}