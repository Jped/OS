#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

void printError(char * action) { 
	fprintf(stderr, "error while %s: %s\n", action, strerror(errno));		
}

void openAndDup(int flags, char * file_name, int new_fd){
	int fd = open(file_name, flags, 0666);
	if(fd<0){
		char error_message[4096];
		snprintf(error_message, 4096,"opening and duplicating %s for io redirect", file_name);
		printError(error_message);
	}else{
		dup2(fd, new_fd);
	}
}

int main() {
	char buffer[4096];
	int buff_loc = 0;
	char letter;
	char * command;
	int exit_num = 0;
	char delim[3] = {'\t', ' ', '\n'};
	while(scanf("%c", &letter) != EOF){
		if (letter == '\n'){
			buffer[buff_loc] = '\0';
			// break up input into pieces
			command = strtok(buffer, delim); 	
			if (!command)
				continue;
			if (strncmp(command, "pwd", 3) == 0) {
				char path_buffer[4096];
				char * resp = getcwd(path_buffer, 4096);
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
				char * next_argument;
				char * argument_list[1024];
				char * file_name;
				struct rusage ru;
				int status;
				int argument_loc = 1;
				switch(pid=fork()){
					case -1: 
						perror("fork failed");
						exit(1);
					case 0:
						// do the exec here and io redirect here.
						argument_list[0] = command;
						while((next_argument = strtok(NULL, delim)) && argument_loc<1024){
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
								printf(" we are here \n");
								openAndDup(O_WRONLY|O_CREAT|O_TRUNC, file_name, 1);
							}else if(strncmp(next_argument, "<", 1) == 0){
								file_name = next_argument + 1;
								openAndDup(O_RDONLY, file_name, 0);
							}else{
								argument_list[argument_loc] = next_argument;
								argument_loc++;	
							}
							
							

						}
						execvp(command, argument_list);
						break;
					default:
						if(wait3(&status, 0, &ru) == -1)
							perror("issue while waiting");
						else{
							// get exit code here
							if(WIFEXITED(status)){
								exit_num = WEXITSTATUS(status);
							}else if(WIFSIGNALED(status)){
								exit_num = WEXITSTATUS(status);
							}
							// get real time

							printf("Child process %d exited with return value %d\n Real:%1d.%.6ds User:%1d.%.6ds Sys:%1d.%.6ds\n", pid, exit_num,
									       (int) ru.ru_utime.tv_sec, (int) ru.ru_utime.tv_usec, 
									       (int) ru.ru_utime.tv_sec, (int) ru.ru_utime.tv_usec, 
									       (int) ru.ru_stime.tv_sec, (int) ru.ru_stime.tv_usec);	
						}						
				}
			}
			//determine if we will fork and execute or direct handeling.
			buff_loc = 0;
		}else{
			buffer[buff_loc] = letter;
			buff_loc++;
		}
	}
	exit(exit_num);
} 
