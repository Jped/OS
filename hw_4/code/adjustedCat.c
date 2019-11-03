#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define MAX_ERROR_LENGTH 4096
#define READ_BUFF_SIZE 4096

int fileError(int type, char * filename, int permission) {
	char action[6], perm[13];
	switch(type) {
		case 0:
			strcpy(action, "open");
			break;
		case 1:
			strcpy(action, "read");
			break;
		case 2:
			strcpy(action, "writ");
			break;
		case 3:
			strcpy(action, "close");
			break;
	}
	switch(permission) {
		case O_WRONLY|O_CREAT|O_TRUNC:
			strcpy(perm, "write/create");
			break;
		case O_RDONLY:
			strcpy(perm, "read");
			break;
		case -1:
			strcpy(perm, "--");
			break;
	}
	fprintf(stderr, "error while %sing file %s for %s: %s\n", action, filename, perm, strerror(errno));	
	return 1;
}

int printError(char * action) { 
        fprintf(stderr, "error while %s: %s\n", action, strerror(errno));               
	return 1;
}

int checkBinary(char * buff, int size) {
	for(int i=0; i<size; i++) {
		if (!(isprint(buff[i]) || isspace(buff[i]))) {
			return 1;
		}
	}
	return 0;
}


int openAndDup(int fd, int new_fd){
	if(dup2(fd, new_fd)<0) {
		char error_message[MAX_ERROR_LENGTH];
		snprintf(error_message, MAX_ERROR_LENGTH, "duping file: %d to new_fd: %d", fd, new_fd);
		printError(error_message);
		return 1;
	}
	if(close(fd)<0){
		char error_message[MAX_ERROR_LENGTH];
		snprintf(error_message, MAX_ERROR_LENGTH, "closing file %d", fd);
		printError(error_message);
		return 1;
	}
	return 0;
}


int grep(char * pattern, int * pipe1, int * pipe2){
	// do the dup
	if(openAndDup(pipe1[0], 0)!=0)
		return 1;
	if(openAndDup(pipe2[1], 1)!=0)
		return 1;
	char * argument_list[3];
	char * program_name = "grep";
	argument_list[0] = program_name;
	argument_list[1] = pattern;
	argument_list[2] = NULL;
	return execvp("grep", argument_list);
}

int more(int input_fd) {
	if(openAndDup(input_fd, 0)!=0)
		return 1;
	char * argument_list[2];
	char * program_name = "more";
	argument_list[0] = program_name;
	argument_list[1] = NULL;
	return execvp("more", argument_list);
}

// Pattern is for grep and infiles is a list of inputed files
int cat(int argc, char ** argv){
    int totalBytes = 0;
    int fileCount = 0;
    int bytesWritten, bytesRead;
    int pipe1[2];
    int pipe2[2];
    char * pattern = argv[1];
    char  * filename;
    char buff[READ_BUFF_SIZE];
    int binary, fdInput;
    // iterate through the files
    for (int i=2;i<argc;i++){
	fileCount++;
	binary = 0;
	if (pipe(pipe1)<0)
		return printError("creating first pipe");
	if (pipe(pipe2)<0)
		return printError("creating second pipe");
	switch(fork()){
		case -1:
			return printError("forking into grep process");
		case 0:
			// now that we are in the grep child lets close
			// all the non relevant fds
			if(close(pipe1[1])<0 || close(pipe2[0])<0)
				return printError("closing dangling file descriptors");
			return grep(pattern, pipe1, pipe2);
	}
	// no one needs pipe1[0] and pipe2[1] so will close
	if(close(pipe1[0])<0 || close(pipe2[1])<0)
		return printError("closing unnecessary grep pipes");
	switch(fork()){
		case -1:
			return printError("forking into more process");
		case 0:
			// close non relevant fds
			if(close(pipe1[1])<0)
				return printError("closing dangling file descriptors in more");
			return more(pipe2[0]);		
	}
	// close pipe2[0]
	if (close(pipe2[0])<0)
		return printError("closing the output pipe for more in parent");
	fdInput = open(argv[i], O_RDONLY);
	filename = argv[i];
	if (fdInput<0)
		return fileError(0, filename, O_RDONLY);
	while(1){
		bytesRead = read(fdInput, buff, READ_BUFF_SIZE);
		if (bytesRead == -1)
			return fileError(1, filename, O_RDONLY);
		else if (bytesRead == 0)
			break;
		bytesWritten = write(pipe1[1], buff, bytesRead);
		// deal with partial write
		while(bytesWritten>0 && bytesRead>bytesWritten) {
			fprintf(stderr, "warning partial write, writing remainder");
			bytesRead = bytesRead - bytesWritten;
			bytesWritten  = write(pipe1[1], buff + bytesWritten, bytesRead);
		}
		totalBytes += bytesWritten;
		if (!binary)
			binary = checkBinary(buff, bytesWritten);
		if (bytesWritten == -1 || (fdInput!=0 && bytesWritten == 0))
			return fileError(2, filename, O_WRONLY|O_CREAT);
	}
	if (binary)
		fprintf(stderr, "\tWarning: this is a binary file\n");
	
	if (close(fdInput)<0)
		return printError("closing input file");
    	if(close(pipe1[1])<0)
		return printError("closing pipe1 input");
    	// wait on both children to close em up.
	wait(NULL);
	wait(NULL);	
    }
}

int main (int argc, char **argv) {
    // make sure enough arguments are passed
    if (argc < 3) {
	fprintf(stderr, "You only provided %d arguments, atleast 3 have to be passed.\nUsage for this script is 'catgrepmore pattern infile1 [...infile2...]'\n", argc);
    	return 1;
    }
    return cat(argc, argv);
}
