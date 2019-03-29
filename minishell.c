#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
    int child_id;

	int numargs;
	int maxargs = 100 * sizeof(char*);

	char **args_perm = malloc(maxargs);
	memset(args_perm, 0, sizeof(args_perm));
	char **args = args_perm;

	//prompt variables
	char *prompt = "\nminishell>> ";

	char *line_addr = malloc(sizeof(char*));
	memset(line_addr, 0, sizeof(line_addr));

	char *tok_addr = malloc(sizeof(char*));
	memset(tok_addr, 0, sizeof(tok_addr));

	//special function call variables
	int bg, redir, pipe_num;

	//file handling variables
	char *file_loc_perm = malloc(100*sizeof(char));
	memset(file_loc_perm, 0, sizeof(file_loc_perm));
	char *file_loc = file_loc_perm;

	int fd[2];

	int stdout_cp = dup(1);
	int stdin_cp = dup(0);

	FILE *f_redir;

	//background process holding variables
	int bgcounter = 0, bgstatus;
	int active[20];
	char **activenames = malloc(20*sizeof(char*));
	memset(activenames, 0, sizeof(activenames));

	//array of arguments sent to redirected method
        char **redirArgs_perm = malloc(maxargs);
	memset(redirArgs_perm, 0, sizeof(redirArgs_perm));
	char **redirArgs = redirArgs_perm;
	
	//misc. variables
	int i = 0;
		
	void setup(){
		args = args_perm;
        memset(args, 0, maxargs);
        numargs = 0;

		bg = 0;
        bgstatus = 0;

		redir = 0;
        f_redir = 0;

		pipe_num = 0;
	}

	int getcmd()
	{
		int *length = malloc(sizeof(int));
		size_t *linecap = malloc(sizeof(size_t));
		int *j = malloc(sizeof(int));
		char *token = tok_addr;
		char *line = line_addr;
		
		printf("%s", prompt);
		*length = getline(&line, linecap, stdin);

		if (*length <= 0){
			exit(-1);
		}
		else if (index(line, '&')!= NULL){
			bg = 1;
		}
		else if (index(line, '>')!= NULL){
			redir = 1;
		}
		else if (index(line, '|')!= NULL){
			pipe_num = 1;
		}
		else{
			bg = 0;
			redir = 0;
			pipe_num = 0;
		}

		while ((token = strsep(&line, " \t\n&")) != NULL) {
			for (*j = 0; *j < strlen(token); *j = *j + 1){
				if (token[*j] <= 32){
					token[*j] = '\0';
				}
			}
			if (strlen(token) > 0){
				args[numargs++] = token;
			}
		}

		if(token)	free(token);
		if(length)	free(length);
		if(linecap)	free(linecap);
		if(j)		free(j);

		return numargs;
	}

	int builtin(){
		if(strcmp(args[0], "cd")==0){ //built in method; uses system call
			chdir(args[1]);
		}
		else if(strcmp(args[0], "fg")==0){ //built in method; moves a process from the bg process array to the front
			if(active[0]==0){
				printf("No process currently running in the background.\n");
			}
			else{
				child_id = active[atoi(args[1])];
				waitpid(child_id, NULL, 0);
			}
		}
		else if(strcmp(args[0], "pwd")==0){ //built in method; uses system call
			printf("%s\n", getcwd(NULL, 0));
		}
		else if(strcmp(args[0], "jobs")==0){ //built in method; reads the array storing active bg processes
			printf("\nACTIVE JOBS\nPOS\tNAME\n");
			for(i = 0; i < bgcounter; i++){
				if(activenames[i]){
					printf("%d\t%s\n", i, activenames[i]);
				}
			}
		}
		else{
			return -1;
		}
	}

	int arrsearch(char* element){
		for(i = 0; i < maxargs/sizeof(char); i++){
			if(strcmp(args[i],element)==0){
				return i;
			}
		}
	}

	void redirect(){ // sets up for executing line of the form: (function) > (output_location)
		i = arrsearch(">");
		file_loc = file_loc_perm;
		memcpy(file_loc, args[i+1], sizeof(args[i+1]));
		f_redir = fopen(file_loc, "w+");		
		dup2(fileno(f_redir), 1);
	}

	void setupPipe(){
		i = arrsearch("|");

		redirArgs = redirArgs_perm;
        memcpy(redirArgs, args, i*sizeof(char*));

		pipe(fd);
	
		child_id = fork();
		if(child_id==0){
			close(fd[0]);
			dup2(fd[1], 1);
			close(fd[1]);
			args = redirArgs_perm;
		}
		else{
			close(fd[1]);
			dup2(fd[0], 0);
			close(fd[0]);
			args = args + i + 1;
		}
	}

	void dochild(){
		if(pipe_num){
			setupPipe();
		}

		if(bg){
			signal(SIGINT, SIG_IGN);
		}

		if(execvp(args[0], args)==-1){
			printf("\nInvalid command: %s", args[0]);
			exit(EXIT_FAILURE);
		}
	}

	void doparent(){
		if(bg){
			active[bgcounter] = child_id;
			memcpy(activenames+bgcounter, args, sizeof(char*));
			printf("Running in background: %s\n", *(activenames+bgcounter));
			bgcounter++;
		}
		else{
			waitpid((pid_t)child_id, NULL, 0);
		}
	}

	void cleanbg(int kill_id){
        for(i = 0; i < bgcounter; i++){
			if(active[i] == bgstatus){
				break;
			}
			if(active[i] == kill_id){
				break;
			}
		}

		for(i; bgcounter > 0 && i < bgcounter; i++){
			*(active + i) = *(active + i + 1);
			*(activenames+i) = *(activenames+i+1);
		}

		bgcounter--;
    }

	void cleanup(){
		if(f_redir!=0){
			fclose(f_redir);
		}

		dup2(stdin_cp, 0);
		dup2(stdout_cp, 1);

		if(bg|redir|pipe_num){
			numargs--;
		}

		for(i = 0; i < numargs; i++){
			free(args[i]);
		}
    }
	
	void sighandler(int signum){
		if(signum == SIGINT){
			kill(child_id, SIGKILL);
			cleanbg(child_id); 
		}
		if(signum == SIGTSTP){
			SIG_IGN;
		}
    }

	signal(SIGTSTP, sighandler);
	signal(SIGINT, sighandler);

	while(1) {	
		setup();
		getcmd();

		if(numargs){
			if(builtin()==-1){
				
				if(strcmp(args[0], "exit")==0){ //built in method; uses system call
					cleanup();
					break;
				}

				if(redir)
					redirect();

				switch(child_id = fork()){
					case -1:
						exit(EXIT_FAILURE);

					case 0:
						dochild();

					default:
						doparent();
				}
			}
		}

		if((bgstatus = waitpid(-1, NULL, WNOHANG)) > 0)
			cleanbg(0);

		cleanup();
	}

	if(tok_addr)		free(tok_addr);
	if(line_addr)		free(line_addr);
	if(file_loc_perm)	free(file_loc_perm);
	if(redirArgs_perm)	free(redirArgs_perm);
	if(args_perm)		free(args_perm);
	if(activenames)		free(activenames);

	exit(EXIT_SUCCESS);
}

