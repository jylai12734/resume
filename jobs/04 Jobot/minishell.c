/*
Justin Lai
I pledge my honor that I have abided by the Stevens Honor System.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m]"

volatile sig_atomic_t interrupted = 0;

// signal handler
void sig_handler(int sig) {
	interrupted = 1;
	fflush(stdout);
}

int main(int argc, char* argv[], char* envp[]) {
	// declare struct sigaction
	struct sigaction action;
	sigset_t block_mask;
	sigaddset(&block_mask, SIGINT);
	action.sa_handler = sig_handler;
	action.sa_mask = block_mask;
	action.sa_flags = 0;

	// install sigaction
	if (sigaction(SIGINT, &action, NULL) == -1) fprintf(stderr, "Error: Cannot register signal handler. %s. \n", strerror(errno));

	// main loop
	while (1) {
		// set interrupted to false
		interrupted = 0;
		// print the current directory in blue
		char* current_directory = getcwd(NULL, 0);
		printf("%s[%s]%s> ", BLUE, current_directory, DEFAULT);
		fflush(stdout);
		free(current_directory);

		// get user input and dynamically store it in cmdline
		char curr = '0';
		char prev = ' ';
		char* cmdline;
		if ((cmdline = malloc(128)) == NULL) fprintf(stderr, "Error: malloc() failed. %s. \n", strerror(errno));
		int index = 0;
		int capacity = 128;
		int arg_count = 0;
		int size = 0;
		while (size = read(0, &curr, 1) && interrupted == 0) {
			// reach the end of user input
			if (size == 0) break;
			// error reading
			else if (size == -1) {
				free(cmdline);
				fprintf(stderr, "Error: Failed to read from stdin. %s. \n", strerror(errno));
				break;
			}
			else {
				if (index == capacity) {
					char* tmp;
					if ((tmp = (char*)malloc(capacity + 128)) == NULL) {
						free(cmdline);
						fprintf(stderr, "Error: malloc() failed. %s. \n", strerror(errno));
						size = -1;
						break;
					}
					for (size_t i = 0; i < capacity; i++) tmp[i] = cmdline[i];
					free(cmdline);
					cmdline = tmp;
					capacity += 128;
				}
				if (curr == '\n') break;
				cmdline[index] = curr;
				index++;

				// count the number of arguments
				if (prev == ' ' && curr != ' ') arg_count++;
				prev = curr;
			}
		}
		// if SIGINT is sent to terminal
		if (interrupted == 1) {
			printf("\n");
			free(cmdline);
			continue;
		}
		// error with read()
		if (size == -1) continue;
		// user enters enter
		if (cmdline[0] == '\0') {
			free(cmdline);
			continue;
		}

		// get the max size of all of the arguments
		int arg_size = 0;
		int max = 1;
		prev = ' ';
		for (size_t i = 0; i <= index; i++) {
			curr = cmdline[i];
			if (i == index || (prev == ' ' && curr != ' ')) {
				if (max > arg_size) arg_size = max;
				max = 1;
			}
			else if (curr != ' ') max++;
			prev = curr;
		}

		// parse the command line and store it in arguments
		char arguments[arg_count][arg_size + 1];	
		int count = 0;
		prev = ' ';
		for (size_t i = 0; i < index; i++) {
			curr = cmdline[i];								
			if (prev == ' ' && curr != ' ') {
				int offset = 0;
				for (size_t j = i; j < index; j++) {
					curr = cmdline[j];
					if (curr == ' ') break;
					arguments[count][offset++] = curr; 
				}
				arguments[count][offset] = '\0';
				i += offset;
				count++;
			}
			prev = curr;
		}
		// free the dynamically allocated command line
		free(cmdline);

		// user enters exit
		if (arg_count == 1 && (strcmp(arguments[0], "exit\0") == 0)) break;
		// user enters pwd
		else if (arg_count == 1 && (strcmp(arguments[0], "pwd\0") == 0)) {
			char* temp;
			if ((temp = getcwd(NULL, 0)) == NULL) fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
			else {
				printf("%s\n", temp);
				free(temp);
			}
		}
		// user enters cd
		else if (strcmp(arguments[0], "cd") == 0) {
			if (arg_count == 1 || (arg_count == 2 && (strcmp(arguments[1], "~") == 0))) {
				struct passwd* pwd;
				if ((pwd = getpwuid(getuid())) == NULL) fprintf(stderr, "Error: Cannot get passwd entry. %s. \n", strerror(errno));
				else {
					char home[PATH_MAX] = {'\0'};
					strcpy(home, "/home/");
					strcat(home, pwd->pw_name);
					chdir(home);
				}
			}
			else if (arg_count == 2) {
				if ((chdir(arguments[1])) == -1) fprintf(stderr, "Error: Cannot change directory to '%s'. %s. \n", arguments[1], strerror(errno));
			}
			else {
				fprintf(stderr, "Error: Too many arguments to cd. \n");
			}
		}
		// user enters something else
		else {
			char* arg_exec[arg_count + 1];
			for (size_t i = 0; i < arg_count; i++) arg_exec[i] = arguments[i];
			arg_exec[arg_count] = NULL;
			char executable_path[PATH_MAX];
			strcpy(executable_path, "/usr/bin/");
			strcat(executable_path, arg_exec[0]);
			pid_t pid;
			int stat;

			// if the fork faisl
			if ((pid = fork()) < 0) fprintf(stderr, "Error: fork() failed. %s. \n", strerror(errno));
			// child process
			else if (pid == 0) { 
				// check if path to the binary file exists, if it does, execute it
				if ((execv(arg_exec[0], arg_exec)) == -1 && (execv(executable_path, arg_exec)) == -1) {
					fprintf(stderr, "Error: exec() failed. %s. \n", strerror(errno));
					return EXIT_FAILURE;
				}
			}
			else { // parent process
				// wait for the child process or wait until SIGINT is sent
				while (interrupted == 0) {
					wait(&stat);
					break;
				}
				// send SIGINT to child process
				if (interrupted == 1) {
					kill(pid, SIGINT);
					printf("\n");
				}
				// if wait fails
				if (stat == -1) fprintf(stderr, "Error: wait() failed. %s. \n", strerror(errno));
			}
		}
		// reset interrupted to false or zero
		interrupted = 0;
	}
	return EXIT_SUCCESS;
}