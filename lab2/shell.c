#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

#define max_size 256
#define TRUE 1
#define FALSE 0

const char* COMMAND_EXIT = "exit";
const char* COMMAND_CD = "cd";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";
const char* COMMAND_PIPE = "|";
const char* COMMAND_OUTPLUS=">>";
const char* COMMAND_PWD="pwd";
const char* COMMAND_wait="&";
int flag;
enum {
	RESULT_NORMAL,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_WRONG_PARAMETER,
	ERROR_MISS_PARAMETER,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_SYSTEM,
	ERROR_EXIT,

	//wrong redirectory
	ERROR_MANY_IN,
	ERROR_MANY_OUT,
	ERROR_FILE_NOT_EXIST,
	
	// wrong pipe
	ERROR_PIPE,
	ERROR_PIPE_MISS_PARAMETER
};

char username[max_size];
char hostname[max_size];
char curPath[max_size];
char commands[max_size][max_size];



void signalhandle(int sig){
    if (sig == SIGINT){
	fflush(stdout);
        printf("\n");
        exit(0);
    }
}

// Used in main(), the signal() func_function
void clear(int sig){
	// clear the buffer
	if(!flag) {
		printf("\n");
		printf("$ ");
		fflush(stdout);		
	}
	else {	
		fflush(stdin);
	}
}

void getUsername() { // get username
	struct passwd* pwd = getpwuid(getuid());
	strcpy(username, pwd->pw_name);
}

void getHostname() { // get hostname
	gethostname(hostname, max_size);
}

int getCurWorkDir() { // get workdirectory
	char* result = getcwd(curPath, max_size);
	if (result == NULL)
		return ERROR_SYSTEM;
	else return RESULT_NORMAL;
}

int splitCommands(char command[max_size]) { // split commands
	int num = 0;
	int i, j;
	int len = strlen(command);

	for (i=0, j=0; i<len; ++i) {
		if (command[i] != ' ') {
			commands[num][j++] = command[i];
		} else {
			if (j != 0) {
				commands[num][j] = '\0';
				++num;
				j = 0;
			}
		}
	}
	if (j != 0) {
		commands[num][j] = '\0';
		++num;
	}

	return num;
}

int callExit() { // exit
	pid_t pid = getpid();
	if (kill(pid, SIGTERM) == -1) 
		return ERROR_EXIT;
	else return RESULT_NORMAL;
}

int isCommandExist(const char* command) { //judge command existency
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	int fds[2];
	if (pipe(fds) == -1) {
		result = FALSE;
	} else {
		// store redirectory flag 
		int inFd = dup(STDIN_FILENO);
		int outFd = dup(STDOUT_FILENO);

		pid_t pid = vfork();
		if (pid == -1) {
			result = FALSE;
		} else if (pid == 0) {
			// redirectory out to file
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);

			char tmp[max_size];
			sprintf(tmp, "command -v %s", command);
			system(tmp);
			exit(1);
		} else {
			waitpid(pid, NULL, 0);
			//  input redirectory 
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO);
			close(fds[0]);

			if (getchar() == EOF) { // no data(command doesn't exist)
				result = FALSE;
			}
			
			//restore input output redirection
			dup2(inFd, STDIN_FILENO);
			dup2(outFd, STDOUT_FILENO);
		}
	}

	return result;
}
int callRedirectory(int x, int y) { 
	if (!isCommandExist(commands[x])) {
		return ERROR_COMMAND;
	}	

	int inNum = 0, outNum = 0,outplusNum=0;
	char *inFile = NULL, *outFile = NULL,*outplusFile=NULL;
	int endIdx = y; 

	for (int i=x; i<y; ++i) {
		if (strcmp(commands[i], COMMAND_IN) == 0) { // input
			++inNum;
			if (i+1 <y)
				inFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; 
			if (endIdx == y) endIdx = i;
		} else if (strcmp(commands[i], COMMAND_OUT) == 0) { // output
			++outNum;
			if (i+1 < y)
				outFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; 
				
			if (endIdx == y) endIdx = i;
		}
		else if(strcmp(commands[i],COMMAND_OUTPLUS)==0){
			++outplusNum;
			if (i+1 < y)
				outplusFile = commands[i+1];
				else return ERROR_MISS_PARAMETER;
				if (endIdx == y) endIdx = i;
		}
	}
	//handle redirectory
	if (inNum == 1) {
		FILE* fp = fopen(inFile, "r");
		if (fp == NULL) // no file
			return ERROR_FILE_NOT_EXIST;
		
		fclose(fp);
	}
	
	if (inNum > 1) { // too many sign
		return ERROR_MANY_IN;
	} else if (outNum > 1) { 
		return ERROR_MANY_OUT;
	}else if(outplusNum>1)
	  return ERROR_MANY_OUT;

	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) {
		// child process redirectory
		if (inNum == 1)
			freopen(inFile, "r", stdin);
		if (outNum == 1)
			freopen(outFile, "w", stdout);
        if(outplusNum==1)
		    freopen(outplusFile,"a+",stdout) ;
		// run command
		char* comm[max_size];
		for (int i=x; i<endIdx; ++i)
			comm[i] = commands[i];
		comm[endIdx] = NULL;
		execvp(comm[x], comm+x);
		exit(errno); 
	} else {
		int status;
		waitpid(pid, &status, 0);
		int errorno = WEXITSTATUS(status); // read the return code 
		if (errorno) { 
			printf("Error: %s\n", strerror(errorno));
		}
	}
	return result;
}
int callPipe(int x ,int y ){ // command region
	if (x >= y) return RESULT_NORMAL;//if has pipe
	int pipeIdx = -1;
	for (int i=x; i<y; ++i) {
		if (strcmp(commands[i], COMMAND_PIPE) == 0) {
			pipeIdx = i;
			break;
		}
	}
	if (pipeIdx == -1) { // no pipe
		return callRedirectory(x, y);
	} else if (pipeIdx+1 == y ){ //| without later
		return ERROR_PIPE_MISS_PARAMETER;
	}

	//run
	int fds[2];
	if (pipe(fds) == -1) {
		return ERROR_PIPE;
	}
	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) { //child process run
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO); // output to fds[1]
		close(fds[1]);
		
		result = callRedirectory(x, pipeIdx);
		exit(result);
	} else { // parent process
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RESULT_NORMAL) { // exit error
			char info[2048] = {0};
			char line[max_size];
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // input to fds[0]
			close(fds[0]);
			while(fgets(line, max_size, stdin) != NULL) { 
				strcat(info, line);
			}
			printf("%s", info); 
			
			result = exitCode;
		} else if (pipeIdx+1 < y){
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // input to fds[0]
			close(fds[0]);
			result = callPipe(pipeIdx+1, y); // recursion
		}
	}

	return result;
}
int calloutCommand(int commandNum) { // handle out command
	pid_t pid = fork();
	if (pid == -1) {
		return ERROR_FORK;
	} else if (pid == 0) {
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);
        signal(SIGINT,signalhandle);
		int result = callPipe(0, commandNum);
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}


int callCd(int commandNum) { // run cd
	int result = RESULT_NORMAL;
	if (commandNum < 2) {
		result = RESULT_NORMAL;
		char *x="/home/";
		char y[max_size];
		strcpy(y,x);
		strcat(y,username);
		chdir(y);
	} else if (commandNum > 2) {
		result = ERROR_TOO_MANY_PARAMETER;
	} else {
		int ret = chdir(commands[1]);
		if (ret) result = ERROR_WRONG_PARAMETER;
	}

	return result;
}

int callPwd(int commandNum)//run pwd
   {
	 char* result = getcwd(curPath,max_size);
	if (result == NULL)
		return ERROR_SYSTEM;
	else 
	{   printf("%s\n",result);
		return RESULT_NORMAL;
	}
   }
void mysignal()
{
	signal(SIGINT,SIG_IGN);
}
int main() {
	//get username ,hostname,workingdirectory
	int result = getCurWorkDir();
	if (ERROR_SYSTEM == result) {
		fprintf(stderr, "Error: System error while getting current work directory.\n");
		exit(ERROR_SYSTEM);
	}
	getUsername();
	getHostname();
    mysignal();
	// start shell
	char argv[max_size];
	while (TRUE) {
		flag=0;
		printf("$ "); 
		//get command
		signal(SIGINT, clear);
		fgets(argv, max_size, stdin);
		flag=1;
		int len = strlen(argv);
		if (len != max_size) {
			argv[len-1] = '\0';
		}
       
		int commandNum = splitCommands(argv);
		
		if (commandNum != 0) { // has input
			if (strcmp(commands[0], COMMAND_EXIT) == 0) { // exit
				result = callExit();
				if (ERROR_EXIT == result) {
					exit(-1);
				}	
			} 
			  else if(strcmp(commands[0],COMMAND_PWD)==0)//pwd
			  {
                 result=callPwd(commandNum);
			  }
			  else if (strcmp(commands[0], COMMAND_CD) == 0) { // cd
				result = callCd(commandNum);
				switch (result) {
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "Error: Miss parameter while using command \"%s\".\n"
							, COMMAND_CD);
						break;
					case ERROR_WRONG_PARAMETER:
						fprintf(stderr, "Error: No such path \"%s\".\n", commands[1]);
						break;
					case ERROR_TOO_MANY_PARAMETER:
						fprintf(stderr, "Error: Too many parameters while using command \"%s\".\n"
							, COMMAND_CD);
						break;
					case RESULT_NORMAL: 
						result = getCurWorkDir();
						if (ERROR_SYSTEM == result) {
							fprintf(stderr
								, "Error: System error while getting current work directory.\n");
							exit(ERROR_SYSTEM);
						} else {
							break;
						}
				}
			}
			  else { // out command
				result = calloutCommand(commandNum);
				switch (result) {
					case ERROR_FORK:
						fprintf(stderr, "Error: Fork error.\n");
						exit(ERROR_FORK);
					case ERROR_COMMAND:
						fprintf(stderr, "Error: Command not exist in shell.\n");
						break;
					case ERROR_MANY_IN:
						fprintf(stderr, "Error: Too many redirection symbol \"%s\".\n", COMMAND_IN);
						break;
					case ERROR_MANY_OUT:
						fprintf(stderr, "Error: Too many redirection symbol \"%s\".\n", COMMAND_OUT);
						break;
					case ERROR_FILE_NOT_EXIST:
						fprintf(stderr, "Error: Input redirection file not exist.\n");
						break;
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "Error: Miss redirect file parameters.\n");
						break;
					case ERROR_PIPE:
						fprintf(stderr, "Error: Open pipe error.\n");
						break;
					case ERROR_PIPE_MISS_PARAMETER:
						fprintf(stderr, "Error: Miss pipe parameters.\n");
						break;
				}
			}
		}
	}
}