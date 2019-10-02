#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

char * sizeOrDeviceToString(struct stat st, char * str) {
	// max str size is 20
	if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)){
		snprintf(str, 20, "%d, %d", major(st.st_dev), minor(st.st_dev));
	}else{
		snprintf(str, 20, "%ld", st.st_size);
	}
	return str;
}

char * fullPath(char * top_directory, char * directory) {
	char * full_path = malloc(strlen(top_directory) + strlen(directory) + 2);
	strcpy(full_path, top_directory);
	full_path[strlen(top_directory)] = '/';
	strncat(full_path + strlen(top_directory) + 1, directory, strlen(directory);
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
        permission_string[10] = '\0';
	return permission_string;
}

int checkTime(time_t secs, int time_option) {
	time_t now = time(NULL);
	int time_diff = (int) (now - secs);
	if(time_option > 0 && time_diff >= time_option)
		return 1;
	if(time_option < 0 && time_diff < (-time_option))
	       return 1;	
	return 0;
}	

void printError(char * full_path, char * action) { 
	fprintf(stderr, "error while %s  %s: %s\n", action, full_path, strerror(errno));		
}

void resolveUid(uid_t uid, char * buffer) { 
	struct passwd * pass = getpwuid(uid);
	if (pass != NULL)
		strcpy(buffer, pass->pw_name);
	else
		sprintf(buffer, "%d", uid);
}

void resolveGid(gid_t gid, char * buffer){
	struct group * gp = getgrgid(gid);
	if (gp != NULL)
		strcpy(buffer, gp->gr_name);
	else
		sprintf(buffer,"%d", gid);
}

int printInfo(char * top_directory, int device_number, char * directory, int * options) {
	int return_value = 0;
	struct stat st;
	char * full_path = fullPath(top_directory, directory);
	int status = lstat(full_path, &st);
	if (status == 0){
		if (device_number != -1 && st.st_dev != device_number && options[1] == 1){
			fprintf(stderr, "note: not crossing mount point at %s\n", full_path);
		}else if (options[0] != 1 || checkTime(st.st_mtime, options[2])){
			char permission_string[11];
			char user_buffer[1024];
			char group_buffer[1024];
			char size_string[20];
			char * time_string = asctime(gmtime(&st.st_mtime));
			time_string[strlen(time_string) - 1] = '\0';
			resolveUid(st.st_uid, user_buffer);
			resolveGid(st.st_gid, group_buffer);
			switch(st.st_mode & S_IFMT) {
				case S_IFREG:
					return_value = 0;
					permission_string[0] = '-';
					break;
				case S_IFDIR:
					return_value = 1;
					permission_string[0] = 'd';
					break;
			}
			printf("%ld\t%ld\t%s\t%ld\t%s\t%s\t%s\t%s\t%s",
						st.st_ino,
						st.st_blocks/2,
						permissionString(permission_string, st),
						st.st_nlink,
						user_buffer,
						group_buffer,
						sizeOrDeviceToString(st, size_string),
						time_string,
						full_path);
			if ((st.st_mode & S_IFMT) == S_IFLNK) {
				char symlink[1024];
				readlink(directory, symlink, 1024);
				printf(" -> %s", symlink);
			}
			printf("\n");
		}
	} else {
		printError(full_path, "opening");
	}
	free(full_path);
	if (device_number == -1)
		return st.st_dev;
	return return_value;
}

int recurseDirectory(char * top_directory, int device_number, int * options) {
	DIR *d;
	struct dirent * dir;
	int type;
	int len_top_directory = strlen(top_directory);
	d = opendir(top_directory);
	int temp;
	if (d) { 
		while ((dir = readdir(d)) != NULL) {
			// call print function 
			if (strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") !=0){
				type = printInfo(top_directory, device_number, dir->d_name, options);
				if (type == 1) {
					fullPath(top_directory, dir->d_name);
					recurseDirectory(top_directory, device_number, options);	
					top_directory[len_top_directory] = '\0';
				}
			}
		}
		closedir(d);
	} else {
		printError(top_directory, "opening dir");
	}
	return 1;
}


int main(int argc, char **argv) { 
	// get the options
	int options[3] = {0, 0, 0}; 	
	int c;
	char top_directory[1024];
	while ((c = getopt(argc, argv, "m:v")) != -1) {
		switch(c) {
			case 'm':
				options[0] = 1;
				options[2] = atoi(argv[optind-1]);
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
	char end[1] = {'\0'};
	int device_number = printInfo(top_directory, -1, end, options);
	int response  = recurseDirectory(top_directory, device_number, options);
	return response;
}
