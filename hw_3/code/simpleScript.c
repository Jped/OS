/******************************************
 * 		Jonathan Pedoeem	  *
 * 	     Coding Homework Number 3	  *
 *          Operating Systems Fall 2019	  *
 * 	        Proffesor J. Hakner	  *
 * ***************************************/

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGUMENT_LENGTH 4096 /* longest each line can be for the shell */
#define MAX_NUM_ARGUMENT 1024 /* highest number of arguments that can be passed in */
#define MAX_ERROR_LENGTH 4096 /* longest error string length*/
#define MAX_EXIT_OR_SIG_STRING 1024 /* longest string describing exit or signal code */

void printError(char * action) { 
	fprintf(stderr, "error while %s: %s\n", action, strerror(errno));		
}

void openAndDup(int flags, char * file_name, int new_fd){
	int fd = open(file_name, flags, 0666);
	if(fd<0){
		char error_message[MAX_ERROR_LENGTH];
		snprintf(error_message, MAX_ERROR_LENGTH,"opening and duplicating %s for io redirect", file_name);
		printError(error_message);
	}else{
		dup2(fd, new_fd);
	}
}

int shell(FILE * input_file) {
	char buffer[MAX_ARGUMENT_LENGTH];
	int buff_loc = 0;
	int comment_line = 0;
	char letter;
	char * command;
	int exit_num = 0;
	int sig_num = -1;
	char delim[4] = {'\t', ' ', '\n', '\0'};
	while(fscanf(input_file, "%c", &letter) != EOF){
		if (letter == '\n'){
			buffer[buff_loc] = '\0';
			buff_loc = 0;
			comment_line = 0;
			// break up input into pieces
			command = strtok(buffer, delim); 	
			if (!command)
				continue;
			if (strncmp(command, "pwd", 3) == 0) {
				char path_buffer[PATH_MAX];
				char * resp = getcwd(path_buffer, PATH_MAX);
				if (!resp)
					printError("pwd");
				printf("%s\n",path_buffer);
			} else if  (strncmp(command, "cd", 2) == 0) {
				// grab the next argument
				char * new_dir = strtok(NULL, delim);
				if (new_dir == NULL){
					new_dir = getenv("HOME");
				}
				int resp = chdir(new_dir);
				if (resp != 0)
					printError("cd");
			} else if  (strncmp(command, "exit", 4) == 0) {
				char * str_exit = strtok(NULL, delim);
				if (str_exit != NULL)
					exit_num = atoi(str_exit);
				exit(exit_num);
			} else {
				int pid;
				struct timeval stop, start;
				char * next_argument;
				char * argument_list[MAX_NUM_ARGUMENT];
				char * file_name;
				struct rusage ru;
				int status;
				int argument_loc = 1;
				switch(pid=fork()){
					case -1: 
						printError("trying to fork");
						exit(1);
					case 0:
						// do the exec here and io redirect here in the child.
						argument_list[0] = command;
						while((next_argument = strtok(NULL, delim)) && argument_loc<MAX_NUM_ARGUMENT){
							// if it is i/o redirection do the io redirect
							if(strncmp(next_argument, "2>", 2) == 0){
								file_name = next_argument + 2;
								openAndDup(O_WRONLY|O_CREAT|O_TRUNC, file_name, 2);
							}else if(strncmp(next_argument, ">>", 2) == 0){
								file_name = next_argument + 2;
								openAndDup(O_WRONLY|O_CREAT|O_APPEND, file_name, 1);
							}else if(strncmp(next_argument, "2>>", 3) == 0){
								file_name = next_argument + 3;
								openAndDup(O_WRONLY|O_CREAT|O_APPEND, file_name, 2);
							}else if(strncmp(next_argument, ">", 1) == 0){
								file_name = next_argument + 1;
								openAndDup(O_WRONLY|O_CREAT|O_TRUNC, file_name, 1);
							}else if(strncmp(next_argument, "<", 1) == 0){
								file_name = next_argument + 1;
								openAndDup(O_RDONLY, file_name, 0);
							}else{
								argument_list[argument_loc] = next_argument;
								argument_loc++;	
							}
							
							

						}
						printf("command %s, argument list %s/n",command, argument_list[1]);
						int resp = execvp(command, argument_list);
						if (resp == -1){
							printError("execing in child");
							exit(127);
						}
						break;
					default:
						gettimeofday(&start, NULL);
						if(wait3(&status, 0, &ru) == -1)
							printError("waiting for child process");
						else{
							// get exit code here
							if(WIFEXITED(status)){
								exit_num = WEXITSTATUS(status);
							}else if(WIFSIGNALED(status)){
								sig_num = WTERMSIG(status);
							}
							char exit_or_sig_string[MAX_EXIT_OR_SIG_STRING];
							if (sig_num != -1) {
								snprintf(exit_or_sig_string, MAX_EXIT_OR_SIG_STRING, "signal %d  (%s)", sig_num, strsignal(sig_num));
								sig_num = -1;
							}else {
								snprintf(exit_or_sig_string, MAX_EXIT_OR_SIG_STRING, "return value %d", exit_num);
							}
							// get real time
							gettimeofday(&stop, NULL);
							printf("Child process %d exited with %s\n Real:%1d.%.6ds User:%1d.%.6ds Sys:%1d.%.6ds\n", pid, exit_or_sig_string,
									       (long) stop.tv_sec - start.tv_sec, (long) stop.tv_usec - start.tv_usec, 
									       (int) ru.ru_utime.tv_sec, (int) ru.ru_utime.tv_usec, 
									       (int) ru.ru_stime.tv_sec, (int) ru.ru_stime.tv_usec);	
						}						
				}
			}
		}else if(!comment_line && buff_loc < MAX_ARGUMENT_LENGTH){
			if (buff_loc == 0 && letter == '#')
				comment_line = 1;
			else{
				buffer[buff_loc] = letter;
				buff_loc++;
			}
		}
	}
	exit(exit_num);
}

int main(int argc, char ** argv) {
	FILE * input_file = stdin;
	if (argc > 1) {
		input_file = fopen(argv[1], "r");
		if (!input_file)
			printError("opening shell script file");
	}
	return shell(input_file);
} 
