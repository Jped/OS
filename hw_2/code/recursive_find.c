#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>

#define CONTINUE 1
#define DONTCONTINUE 0

unsigned int getBlocks(int blksize, int blocks) {
	return (blksize * blocks)/1024;
}

char * resolveSize(struct stat st, char * str) {
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)){
		sprintf(str, "%d, %d", major(st.st_dev), minor(st.st_dev));
	}else{
		sprintf(str, "%ld", st.st_size);
	}
	return str;
}

char * fullPath(char * top_directory, char * directory, struct stat st) {
	char * full_path = malloc(strlen(top_directory) + strlen(directory));
	strcpy(full_path, top_directory);
	strcpy(full_path + strlen(top_directory), directory);
	return full_path;
}

char * permissionString(char * permission_string, struct stat st) {
	permission_string[1] = (st.st_mode & S_IRUSR) ? 'r' :'-';
	permission_string[2] = (st.st_mode & S_IWUSR) ? 'w' :'-';
	permission_string[3] = (st.st_mode & S_IXUSR) ? 'x' :'-';
	permission_string[4] = (st.st_mode & S_IRGRP) ? 'r' :'-';
	permission_string[5] = (st.st_mode & S_IWGRP) ? 'w' :'-';
	permission_string[6] = (st.st_mode & S_IXGRP) ? 'x' :'-';
	permission_string[7] = (st.st_mode & S_IROTH) ? 'r' :'-';
	permission_string[8] = (st.st_mode & S_IWOTH) ? 'w' :'-';
	permission_string[9] = (st.st_mode & S_IXOTH) ? 'x' :'-';
	return permission_string;
}

int printInfo(char * top_directory, char * directory, int * options) {
	struct stat st;
	lstat(directory, &st);
	char permission_string[11];
	char size_string[20];
	int return_value = DONTCONTINUE;
	char * time_string = asctime(gmtime(&st.st_mtime));
	time_string[strlen(time_string)-1]  = '\0';
	char * full_path = fullPath(top_directory, directory, st);
	switch(st.st_mode & S_IFMT) {
		case S_IFREG:
			return_value = DONTCONTINUE;
			permission_string[0] = '-';
			break;
		case S_IFDIR:
			return_value = CONTINUE;
			permission_string[0] = 'd';
			break;
	}
	printf("%ld\t%d\t%ld\t%s\t%s\t%s\t%s\t%s\t%s",
				st.st_ino,
				getBlocks(st.st_blksize, st.st_blocks),
				st.st_nlink,
				permissionString(permission_string, st),
				getpwuid(st.st_uid)->pw_name,
				getgrgid(st.st_gid)->gr_name,
				resolveSize(st, size_string),
				time_string,
				full_path);
	if ((st.st_mode & S_IFMT) == S_IFLNK) {
		char symlink[1024];
		readlink(directory, symlink, 1024);
		printf(" -> %s", symlink);
	}
	printf("\n");
	return return_value;
}

int recurseDirectory(char * top_directory, char * directory, int * options) {
	DIR *d;
	struct dirent * dir;
	int type;
	int len_top_directory = strlen(top_directory);
	d = opendir(directory);
	int temp;
	if (d) { 
		while ((dir = readdir(d)) != NULL) {
			// call print function 
			if (strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") !=0){
				type = printInfo(top_directory, dir->d_name, options);
				if (type == CONTINUE) {
					strcpy(top_directory+ len_top_directory, dir->d_name);
					temp = strlen(top_directory);
					top_directory[temp] = '/';
					top_directory[temp + 1] = '\0';
					recurseDirectory(top_directory, dir->d_name, options);	
					top_directory[len_top_directory] = '\0';
				}
			}
		}
		closedir(d);
	}
}


int main(int argc, char **argv) { 
	// get the options
	int options[3] = {0, 0, 0}; 	
	int c;
	char top_directory[1024];
	while ((c = getopt(argc, argv, "m:v:")) != -1) {
		switch(c) {
			case 'm':
				options[0] = 1;
				break;
			case 'v':
				options[1] = 1;
				break;
			case '?':
				fprintf(stderr, "error: unknown argument");
				return -1;
				break;
		}
	}
	strcpy(top_directory, argv[optind]);
	int response  = recurseDirectory(top_directory, argv[optind], options);
	return response;
}
