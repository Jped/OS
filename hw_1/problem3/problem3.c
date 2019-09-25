#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

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
	return -1;
}

int checkBinary(char * buff, int size) {
	for(int i=0; i<size; i++) {
		if (!(isprint(buff[i]) || isspace(buff[i]))) {
			return 1;
		}
	}
	return 0;
}

int main (int argc, char **argv) {
    int c, fdInput, bytesRead, bytesWritten, readCount, writeCount, totalBytes, binary, close_response;
    int fdOutput = 1;
    int noInput = 0;
    char buff[4096];
    char * filename;
    while ((c = getopt(argc, argv, "o:")) != -1) {
        switch(c) {
		case 'o':
			fdOutput = open(argv[optind-1], O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if (fdOutput == -1)
			 	return fileError(0, argv[optind-1], O_WRONLY|O_CREAT|O_TRUNC);
			break;
		case '?':
			fprintf(stderr, "error: unknown argument");
			return -1;
			break;
	}
    }
    // now read all the input files.
    // for case where no inputs are given
    if (argc == optind) {
	    optind--;
	    noInput = 1;
    }

    for (int i=optind;i<argc;i++){
	readCount = 0;
	writeCount = 0;
       	totalBytes = 0;
	binary = 0;
	if (*argv[i] == '-' || noInput) {
		fdInput = 0;
		noInput = 0;
		char name[17] =  "<standard input>";
		filename = name;
	}else{
		fdInput = open(argv[i], O_RDONLY);
		filename = argv[i];
		if (fdInput == -1)
			return fileError(0, filename, O_RDONLY);
	}
	while(1){
		bytesRead = read(fdInput, buff, 4096);
		readCount++;
		if (bytesRead == -1)
			return fileError(1, filename, O_RDONLY);
		else if (bytesRead == 0)
			break;
		bytesWritten = write(fdOutput, buff, bytesRead);
		// deal with partial write
		while(bytesWritten>0 && bytesRead>bytesWritten) {
			fprintf(stderr, "warning partial write, writing remainder");
			bytesRead = bytesRead - bytesWritten;
			bytesWritten  = write(fdOutput, buff + bytesWritten, bytesRead);
		}
		totalBytes += bytesWritten;
		writeCount++;
		if (!binary)
			binary = checkBinary(buff, bytesWritten);
		if (bytesWritten == -1 || (fdInput!=0 && bytesWritten == 0))
			return fileError(2, filename, O_WRONLY|O_CREAT);
	}
	fprintf(stderr, "Concatenated File: %s,\n\t Total Bytes: %d,\n\t Read Calls: %d,\n\t Write Calls: %d \n",
				filename,
				totalBytes,
				readCount,
				writeCount);
	if (binary)
		fprintf(stderr, "\tWarning: this is a binary file\n");
	if (fdInput!=0) {
		close_response = close(fdInput);
		if (close_response == -1)
			return fileError(3, filename, -1);
	}		
    }
   return 0;
}
