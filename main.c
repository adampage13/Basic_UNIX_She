#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>

// CSC360 - Assignment 1
// Adam Page - V00910612
// June 6, 2020
//
// Basic UNIX-based shell
// 	Supports arbitrary UNIX commands using execcvp()
// 	Supports changing directories
// 	Supports background execution of up to 5 processes
// 	Supports suspending and resuming background processes


void str_to_arr(char token, char *input,char *args[]); 
int token_count(char token,char *input); 
void bg_list(pid_t processes[],char *proc_names[],char proc_status[]);
void bg_kill(int job,pid_t proc_pids[],char proc_status[]);
int bg_check(pid_t proc_pids[]);
void bg_stop(int job,pid_t proc_pids[],char proc_status[]);
void bg_start(int job,pid_t proc_pids[],char proc_status[]);

#define MAX_DIR_SIZE 256
#define MAX_BG_PROC 5

int main (){

	pid_t proc_pids[MAX_BG_PROC]; // array to keep track background process
	int proc_count; 			  // integer to keep track of currently running number of background processes
	
	// Initialize process pids
	for (proc_count=0;proc_count<MAX_BG_PROC;proc_count++){
		proc_pids[proc_count] = 0;
	}
	char *proc_names[MAX_BG_PROC]; // array to hold names and cwd's for background commands
	char proc_status[MAX_BG_PROC]; // array to hold status for background processes
	char prompt[MAX_DIR_SIZE]; 	   // string for shell prompt
	char *cmd;
	int background; 			   // background command flag


	// Main Loop
	for(;;){
		int status;
		pid_t pid;

		// check background processes
		proc_count = bg_check(proc_pids);

		// display shell prompt
		printf("%s",getcwd(prompt,MAX_DIR_SIZE));
		cmd = readline(">");

		// Process input for execvp() call
		int param_num = token_count(' ',cmd);
		char *args[param_num];
		str_to_arr(' ',cmd,args);

		// check for background specifier
		background = 0; 			 // 0 indicates background not request by user
		if(strcmp(args[0],"bg")==0){ // trim bg off args
			background = 1;
			//edit args
			int i=0;
			while(args[i] != NULL){
				args[i-1] = args[i];
				i++;
			} args[i-1] = '\0';
		}

		// BUILT-IN COMMANDS (Non-fork)
		// "cd" command
		if (!strcmp(args[0],"cd")){
			if (param_num-2 == 1){
				// check for  '~' parameter
				if (!strcmp(args[1],"~")){
					int i,count = 0;
					char temp[MAX_DIR_SIZE];
					getcwd(temp,MAX_DIR_SIZE);
					for (i=0;i<MAX_DIR_SIZE;i++){
						if (temp[i] == '/'){
							count++;
						}
						temp[i]='\0';
					}		
					for (i=0;i<count-1;i++){
						strncat(temp,"../",3);
					}
					args[1] = temp;
				}
				if(chdir(args[1]) == -1){
					fprintf(stderr,"Error: path not found\n");
				}
			}else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		// "stop" command
		else if(!strcmp(args[0],"stop")){
			if (param_num-2 == 1){
				bg_stop((int)args[1][0]-'0',proc_pids,proc_status);
			}
			else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		// "start" command
		else if (!strcmp(args[0],"start")){
			if(param_num-2==1){
				bg_start((int)args[1][0]-'0',proc_pids,proc_status);
			}else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		// "bglist" command
		else if(!strcmp(args[0],"bglist")){
			if(param_num-2==0){
				bg_list(proc_pids,proc_names,proc_status);
			}else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		// "bgkill" command
		else if(!strcmp(args[0],"bgkill")){
			if(param_num-2 == 1){
				bg_kill((int)args[1][0]-'0',proc_pids,proc_status);
			}else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		// "exit" command
		else if(!strcmp(args[0],"exit")){
			if (param_num-2 == 0){
				printf("Shell exiting..\n");
				free(cmd);
				return 0;
			}else{
				fprintf(stderr,"Error: incorrect amount of parameters\n");
			}
		}
		
		// FORK COMMANDS
		else{
			if (background == 0 || proc_count < 5){
				if ((pid = fork()) < 0){ // Validity check
					fprintf(stderr,"Error creating child process.\n");
					free(cmd);
					return -1;
				}
				// Child process
				else if (pid  == 0){
					int exec_ret = execvp(args[0],args); //child process terminates here

					fprintf(stderr,"Error in executing command in execvp() call: %d\n",exec_ret);
					free(cmd);
					return -1;
				}
				// Parent processes
				else{
					// background parent
					if (background == 1){
						// add job to background process array
						int i;
						for (i=0;i<MAX_BG_PROC;i++){
							if (proc_pids[i] == 0){
								proc_pids[i] = pid;
								char temp[MAX_DIR_SIZE];
							  getcwd(temp,MAX_DIR_SIZE);
								proc_names[i] = temp;
								strncat(proc_names[i],"/",1);
								strncat(proc_names[i],args[0],strlen(args[0]));

								proc_status[i] = 'R'; // add running indicator R
								break;
							}
						}
					}
					// foreground parent
					else{
						wait(&status); //process waits here for the child to complete
					}
				}
			}
			else {
				fprintf(stderr,"Maximum number of background processes already running\nPlease wait for a process to finish or terminate background command with'bgkill(job number)'\n");
			}
		}
		free(cmd);
	}
	return 0;
}


//---------------------FUNCTIONS---------------------------------------------------------------------------------------------------------

// FUNCTION: token_counter and str_to_arr
// token_counter determines how many strings are in the command inputted by user
// str_to_arr using that number from token_counter to create an array of strings from the input command
//
int token_count(char token,char input[]){
	int count=0,i=0;
	while(input[i] != '\0'){
		if(input[i] == token){
			count++;
		}
		i++;
	} count += 2; 		//add two spaces to args array. One for first string (shell command), and one for null terminator
	return count;
}
void str_to_arr(char token,char input[],char *args[]){
	char delim[2] = {token,'\0'};
	int i = 0;
	char *item = strtok(input,delim);
	while(item != NULL){
		args[i] = item;
		i++;
		item = strtok(NULL,delim);
	}
	args[i] = '\0';
}

// Function: bg_list()
//	handles "bglist" shell command
//	called by user to get status of running background processes
//
void bg_list(pid_t processes[],char *proc_names[],char proc_status[]){
	int i,count = 0;
	for (i = 0;i<MAX_BG_PROC;i++){
		if (processes[i]>0){
			printf("%d[%c]: %s\n",i,proc_status[i],proc_names[i]);
			count++;
		}
	}
	printf("Total Background jobs: %d\n",count);
}

// Function: bg_kill()
//	handles bgkill shell command
// 	terminates a background process specified by the "job" integer
//
void bg_kill(int job, pid_t proc_pids[],char proc_status[]){
	if (proc_pids[job] > 0){ 		    //check to ensure a job is actually running at the given job number
		if (proc_status[job] == 'S'){   //if the job is stopped, start it
			bg_start(job,proc_pids,proc_status);
		}
		kill(proc_pids[job],SIGTERM);
	}
	else{
		fprintf(stderr,"Error: no running process at job [%d]\n",job);
	}
}

// Function: bg_check()
//	Checks the status of running background PROCESSES
//	Reports termination to the user
//	updates the process array upon termination
//	returns number of currently active processes
// 
int bg_check(pid_t proc_pids[]){
	int i,count = 0;
	pid_t curr;
	for (i=0;i<MAX_BG_PROC;i++){
		if(proc_pids[i] > 0){
			curr = waitpid(proc_pids[i],NULL,WNOHANG);
			if ((unsigned long int)curr > 0){ 		// the process has ended. waitpid returns child pid upon its termination. Uses unsigned int to include waitpid returning an error as termination
				printf("Job [%d] has ended\n",i);
				proc_pids[i] = 0;
			}
			else{
				count++; //count how many active processes
			}
		}
	}
	return count;
}

// Function: bg_stop()
//	handles "stop" shell command
//	stops a running background process
//
void bg_stop(int job,pid_t proc_pids[],char proc_status[]){
	int status;
	if(proc_pids[job]>0){ 			// ensure there is an active process at this job number
		waitpid(proc_pids[job],&status,WUNTRACED | WCONTINUED | WNOHANG);

		if (WIFSTOPPED(status)){ 	// check if process already stopped
			fprintf(stderr,"Error: Job [%d] is already stopped\n",job);
		}
		else{
			kill(proc_pids[job],SIGSTOP); // send stop signal
			proc_status[job] = 'S';
		}
	}
	else{
		fprintf(stderr,"Error: no process at job number: %d\n",job);
	}
}


// Fuction: bg_start()
//	handles "start" shell command
//  	starts a stopped background process
// 
void bg_start(int job,pid_t proc_pids[],char proc_status[]){
	int status;
	if(proc_pids[job]>0){ // ensure there is a process at this job number
		waitpid(proc_pids[job],&status,WUNTRACED | WCONTINUED | WNOHANG);

		if (WIFCONTINUED(status)){ 			// check if process already stopped
			fprintf(stderr,"Error: Job [%d] is already running\n",job);
		}
		else if(proc_status[job] == 'R'){ 	// catch for the case when "start" command is issue to a background process that has not been stopped yet. Therefore the WIFCONTINUNED macro will not catch it
			fprintf(stderr,"Error: Job [%d] is already running\n",job);
		}
		else{
			kill(proc_pids[job],SIGCONT); 	// send stop signal
			proc_status[job] = 'R';
		}
	}
	else{
		fprintf(stderr,"Error: no process at job number: %d\n",job);
	}
}
