#include "p1fxns.h"
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>


//number of processes and processors
int nprocesses  = -1; 
int nprocessors = -1;

//command string
char *command = NULL;

//time vars
struct timespec start, end;

//args to send to execvp
char **args  = NULL;
int argsSize = 0;


//routine for exiting program
void exitRoutine(int status){
    clock_gettime(CLOCK_REALTIME, &end);
    
    long int tmp_nsecs, secs, msecs;
    
    tmp_nsecs = end.tv_nsec - start.tv_nsec;
    
    secs  = (tmp_nsecs > 0) ? (long int) (end.tv_sec - start.tv_sec) :  (long int) ((end.tv_sec - start.tv_sec) - 1);
    
    msecs = (tmp_nsecs > 0) ? (long int) ((tmp_nsecs)/1000000) : (long int) ((1000000000 + (tmp_nsecs))/1000000);

    char msecs_str[32];
    char secs_str[32];
    
    p1itoa(secs, secs_str);
    p1itoa(msecs, msecs_str);
    
    secs_str[7]  = '\0';
    msecs_str[3] = '\0';

    p1putstr(1, "The elapsed time to execute ");
    p1putint(1, nprocesses);
    p1putstr(1, " copies of \"");
    p1putstr(1, command);
    p1putstr(1, "\" on ");
    p1putint(1, nprocessors);
    p1putstr(1, " processors is ");
    p1putstr(1, secs_str);
    p1putstr(1, ".");
    if(msecs < 100){
        p1putstr(1, "0");
        if(msecs < 10){
		    p1putstr(1, "0");
		}
	}
    p1putstr(1, msecs_str);
    p1putstr(1, "sec\n");
           
    if(command != NULL)
        free(command);
    exit(status);
}



void usage(){
    p1perror(2, "USAGE: program [--number=<nprocesses>] [--processors=<nprocessors>] --command='command'\n");
    exit(1);
}

void parseCommands(){

    char buffer[256];
    int location = 0;
    int i = 0;
    
    while(command[location] != '\0'){
   	location = p1getword(command, location, buffer);
   	i++;
    }

    argsSize = i+1;
    
    args = (char**) malloc(sizeof(char*)*argsSize);
    location = 0;
    i = 0;
    while(command[location] != '\0'){
        location = p1getword(command, location, buffer);
        args[i++] = p1strdup(buffer);
    }
    args[i] = NULL;
}

void parseArgs(int argc, char* argv[])
{
    int i;
	
	int location = -1;
	
    for(i = 1; i < argc; i++){
        if(p1strneq(argv[i], "--number=", p1strlen("--number=")) == 1){
            if((location = p1strchr(argv[i], '=')) == -1){
                usage();
            }

            // use p1atoi to convert str to int
            nprocesses = p1atoi(&(argv[i][location+1]));
 
        }else if(p1strneq(argv[i], "--processors=", p1strlen("--processors=")) == 1){
            if((location = p1strchr(argv[i], '=')) == -1){
                usage();
            }

            nprocessors = p1atoi(&(argv[i][location+1]));
    
        }else if(p1strneq(argv[i], "--command=", p1strlen("--command=")) == 1){
            command = (char *) malloc(p1strlen(argv[i])+1);
            command[0] = '\0';
            if((location = p1strchr(argv[i], '=')) == -1){
                usage();
            }

            p1strcpy(command, &(argv[i][location+1]));
        }else{
            usage();
        }
    }
    parseCommands();   
    if(getenv("TH_NPROCESSES") == NULL && nprocesses == -1){
        p1perror(2, "TH_NPROCESSES not given or in env vars!\n");
        exit(0);
    }
    if(getenv("TH_NPROCESSORS") == NULL && nprocessors == -1){
        p1perror(2, "TH_NPROCESSORS not given or in env vars!\n");
        exit(0);
    }

    if(nprocesses == -1)
        nprocesses  = p1atoi(getenv("TH_NPROCESSES"));
    if(nprocessors == -1)
        nprocessors = p1atoi(getenv("TH_NPROCESSORS"));
}






int main(int argc, char* argv[])
{
    int i;


    parseArgs(argc, argv);


    pid_t pid[nprocesses];

    int status, s, sig;
    
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGSTOP);
    sigaddset(&set, SIGCONT);

    s = sigprocmask(SIG_BLOCK, &set, NULL);

    if(s != 0)
        p1perror(2, "error in sigprocmask creation!!\n");
 

    for(i = 0; i < nprocesses; i++){
       
        if((pid[i] = fork()) == 0){

            status = sigwait(&set, &sig); 

            signal(SIGCONT, SIG_DFL);


            if(status != 0)
                p1perror(2, "error in sigwait!\n");

            if(status != 0)
                p1perror(2,"sig_unblock didn't work\n");

            if(execvp(args[0], args) < 0){
                 exit(1);
            }
            
        }
    }

    status = sigprocmask(SIG_UNBLOCK, &set, NULL);

    clock_gettime(CLOCK_REALTIME, &start);

    for(i = 0; i < nprocesses; i++){
        kill(pid[i], SIGUSR1);
    }


    for(i = 0; i < nprocesses; i++){
        kill(pid[i], SIGSTOP);
    }


    for(i = 0; i < nprocesses; i++){
        kill(pid[i], SIGCONT);
    }

    for(i = 0; i < nprocesses; i++){
        waitpid(pid[i], &status, 0);
    }


    exitRoutine(EXIT_SUCCESS);

    return 1;

}