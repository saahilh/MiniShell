#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
	
#define MAX_ARGS	100
#define PROMPT		"\nminishell>> "
#define	MAX_BG_PROCESSES	20

typedef struct active_process_t{
	int pid;
	char* name;
} ActiveProcess;

char* args_array[MAX_ARGS];
char** args = args_array;

char* bg;

FILE* f_redir;
int bgcounter = 0;
ActiveProcess active[MAX_BG_PROCESSES];

int getcmd(){
	int num_args = 0;
	
	printf("%s", PROMPT);
	
	size_t length = 0;
	char* user_input = NULL;
	getline(&user_input, &length, stdin);

	if(length <= 0){
		exit(-1);
	}
	else{
		bg = index(user_input, '&');
	}

	char* token;
	while((token = strsep(&user_input, " \t\n&")) != NULL){
		if(strlen(token) > 0){
			args[num_args++] = token;
		}
	}
	
	free(user_input);
	return num_args;
}

int arr_search(char* element, int num_args){
	for(int i = 0; i < num_args; i++){
		if(strcmp(args[i], element)==0){
			return i;
		}
	}
	return -1;
}

void setup_pipe(int num_args){
	int created_pipe[2];
	pipe(created_pipe);
	
	int pipe_character_position = arr_search("|", num_args);
	int child_id = fork();
	
	switch(child_id){
		case -1:
			exit(EXIT_FAILURE);
		case 0:
			close(created_pipe[0]);
			dup2(created_pipe[1], 1);
			close(created_pipe[1]);

			char** new_args;
			memcpy(new_args, args, pipe_character_position*sizeof(char*));
			args = new_args;
		default:
			close(created_pipe[1]);
			dup2(created_pipe[0], 0);
			close(created_pipe[0]);
			args = &args[pipe_character_position+1];
	}
}

void cleanbg(int kill_id){
	int i;
	
	for(i = 0; i < bgcounter; i++){
		if(active[i].pid == kill_id){
			break;
		}
	}

	if(bgcounter > 1){
		ActiveProcess* new_active_list[MAX_BG_PROCESSES - i];
		memcpy(new_active_list, active + i * sizeof(ActiveProcess), sizeof(new_active_list));
		memset(active + i*sizeof(ActiveProcess), 0, sizeof(ActiveProcess) + sizeof(new_active_list));
		memcpy(active + i*sizeof(ActiveProcess), new_active_list, sizeof(new_active_list));
		
		bgcounter--;
	}
	else{
		bgcounter = 0;
		memset(active, 0, sizeof(active));
	}
	
}

int handle_next_command(int num_args){
	if(num_args > 0){
		char* command = args[0];
		
		if(arr_search(">", num_args)!=-1){
			int redirect_char_location = arr_search(">", num_args);
			char* target_file_name = args[redirect_char_location+1];
			f_redir = fopen(target_file_name, "w+");		
			dup2(fileno(f_redir), 1);
			
			char* new_args[redirect_char_location];
			for(int i = 0; i < redirect_char_location; i++){
				new_args[i] = args[i];
			}
			args = new_args;
			num_args = redirect_char_location;
		}
		
		if(strcmp(command, "cd")==0){ //built in method; uses system call
			chdir(args[1]);
		}
		else if(strcmp(command, "pwd")==0){ //built in method; uses system call
			printf("%s\n", getcwd(NULL, 0));
		}
		else if(strcmp(command, "exit")==0){ //built in method; uses system call
			exit(EXIT_SUCCESS);
		}
		else if(strcmp(command, "ls")==0){ //built in method; uses system call
			if(num_args = 1){
				args[1] = ".";
			}
			DIR *d;
		
			struct dirent *dir;
			d = opendir(args[1]);
			if(d){
				while(dir = readdir(d)){
					printf("%s\t", dir -> d_name);
				}
				closedir(d);
			}
		}
		else if(strcmp(command, "jobs")==0){ //built in method; reads the array storing active bg processes
			printf("\nACTIVE JOBS\nPOS\tNAME\n");
			
			for(int i = 0; i < bgcounter; i++){
				printf("%d\t%s\n", i, active[i].name);
			}
		}
		else if(strcmp(command, "fg")==0){ //built in method; moves a process from the bg process array to the front
			if(active[0].pid==0){
				printf("No process currently running in the background.\n");
			}
			else{
				waitpid(active[atoi(args[1])].pid, NULL, 0);
			}
		}
		else{
			int child_id = fork();
			
			switch(child_id){
				case -1:
					exit(EXIT_FAILURE);
				case 0:
					if(arr_search("|", num_args)!=-1){
						setup_pipe(num_args);
					}
					else if(bg){
						signal(SIGINT, SIG_IGN);
					}
					
					if(execvp(args[0], args)==-1){
						printf("\nInvalid command: %s", args[0]);
						exit(EXIT_FAILURE);
					}
				default:
					if(bg){
						active[bgcounter].pid = child_id;
						active[bgcounter].name = args[0];
						printf("Running in background: %s\n", active[bgcounter].name);
						bgcounter++;
					}
					else{
						waitpid((pid_t) child_id, NULL, 0);
					}
			}
		}
	}
	
	return 0;
}

void sighandler(int signum){
	if(signum == SIGINT){
		int pid = getpid();
		kill(pid, SIGKILL);
		cleanbg(pid);
	}
	if(signum == SIGTSTP){
		SIG_IGN;
	}
}

int main(void){
	int stdin_cp = dup(0);
	int stdout_cp = dup(1);
	
	signal(SIGTSTP, sighandler);
	signal(SIGINT, sighandler);

	while(1) {	
		f_redir = 0;
		memset(args_array, 0, sizeof(args_array));
		args = args_array;
		
		dup2(stdin_cp, 0);
		dup2(stdout_cp, 1);
		
		int num_args = getcmd();
		handle_next_command(num_args);
		
		if(f_redir!=0){
			fclose(f_redir);
		}
		
		// Non-blocking check to see if any child process has terminated
		int bg_status = waitpid(-1, NULL, WNOHANG);
		
		if(bg_status > 0){
			cleanbg(bg_status);
		}
	}
}