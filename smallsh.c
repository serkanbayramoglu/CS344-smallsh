// Author:      Serkan Bayramoglu
// Email:       bayramos@oregonstate.edu
// Date:        July 24, 2022
// Description: This project provides a small shell to the user. The program has 3 built in functions; cd for changing 
//              the working directory, status for providing the exit value of the termination signal number of the most recent 
//              background process, and  exit for exiting the proram. The program uses execvp() to execute all other command 
//              line commands using child processes. The program allows for a maximum length of 2048 characters consisting of 
//              a maximum of 512 arguments (incl. input / output redirections and background processing symbols, and the 
//              commands. The character '<' stands for input redirection, '>' for output redirection, and '&' (if at the end) 
//              for background processing. the '<' and '>' characters can be in any order, each followed by their respective 
//              file names,before & (if there is any background processing request), however, after all other arguments. 
//              Otherwise they are considered as ordinary strings to be used as arguments for the previous commands. The 
//              program allows for consecutive spaces before, in between or after the arguments, which are not taken into 
//              consideration. However, there must be at least one space between each argument. Built in commands cannot run 
//              on background. Any command ending with & (other than the built in commands) will run at the background while 
//              the rest run on the foreground. Number of processes running on the background cannot exceed 100. One or more 
//              process have to be completed or terminated in order to start a new process on the backgroun. For redirection 
//              of standard input and / or output. The dup2() function is used in the program. Spaces inside arguments are not 
//              allowed, and quoting is not supported. The | pipe operator is also not implemented. The program does not take 
//              into consideration the blank lines when entered, and any lines that start with '#' character. '$$' is the 
//              expansion string, which is replaced by the program with the pid number of the parent process. The SIGINT signal
//              is ignored by the parent process and any background processes. However, when received by the foreground child
//              process, the child process kills itself, and afterwards the parent process prints out the number of the signal
//              that killed the foreground child process before prompting the user for the next input. All foreground and 
//              background processes ignore the SIGSTP signal, while the parent, each time received while the parent is active, 
//              the shell switches alternatively between normal mode and foregrounf mode. When the shell is in the foreground
//              mode, even the user entries ending with '&' are processed in the foreground, and no new background process is 
//              accepted.  


#define _POSIX_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

// Constants
#define MAX_LEN 2048
#define MAX_ARG 512

// Global variables to be used by both, handlers and main, and make all communicate
int fgOnlyMode;
int signalInter;
pid_t childPids [101] = {0};


// Handler for SIGNCHLD to print pid numbers and termination signal numbers on stdout when signal is received on child processes
void handle_SIGCHLD(int signo){
  if (signo != 0) {
    for(int childNo2 = 0; childNo2 < 100; childNo2++) { 
      if (childPids[childNo2] > 0) {
        pid_t w = -5;
        int childStatus2;
        w = waitpid(childPids[childNo2], &childStatus2, WNOHANG);
        if (w < 0) {
          perror("waitpid");
          exit(1);
        }
        if (w > 0) { 
          if(WIFEXITED(childStatus2)){
            printf("background pid %d is done: exit value %d\n", childPids[childNo2], WEXITSTATUS(childStatus2));
            fflush(stdout);
          } 
          if (WIFSIGNALED(childStatus2)) {
            printf("background pid %d is done: terminated by signal %d\n", childPids[childNo2], WTERMSIG(childStatus2));
            fflush(stdout);
          }
          childPids[childNo2] = 0 ;
        }
      }
    } 
  }
  signalInter = 1;   // tell perror on row 163, that the cause for the err message is the SGCHLD signal, not the fgets() above it
}  
  
// Handler for SIGNTSTP to manage switcing to and from the foreground only mode
void handle_SIGTSTP(int signo){
  char* message1 = "\nEntering foreground-only mode (& is now ignored)\n";
  char* message2 = "\nExiting foreground-only mode\n";
  if (fgOnlyMode) {
    write(1,message2,30);
    fgOnlyMode = 0;
  } else {
    write(1,message1,50);
    fgOnlyMode = 1;
  }
  signalInter = 1;   // same as row 80 above, used to tell perror that the err message is related to the signal received
}


int main() {
  // List of the variables for the main func
  char readBufferInit [MAX_LEN + 2];
  pid_t mainpid = -5;
  ssize_t readLength;
  int argSize;
  char ch[2];
  int j;
  char mainpidstr[10];
  int childNo = 0;
  int fd_i, fd_o;
  int lastStatusType = 0;         // 0 if exit, 1 if terminated, to be used by the status command
  int lastStatus = 0;
  
  // Initializations
  fgOnlyMode = 0;
  signalInter = 0;
  sigset_t signal_set_ch;
  sigemptyset (&signal_set_ch);
  sigaddset(&signal_set_ch, SIGTSTP);

  // Obtaining process id of the parent
  mainpid = getpid();

  // Converting int to string, to be used to replace $$, as strcat() does not take integer args
  sprintf(mainpidstr, "%d", mainpid);   

  // Creating signal handlers for SIGTSTP and SIGCHLD
  struct sigaction ignore_action = {0}, SIGTSTP_action = {0}, SIGCHLD_action = {0};

  // Filling out the SIGTSTP_action struct, Registering handle_SIGTSTP as the signal handler, No flags set
  SIGTSTP_action.sa_handler = handle_SIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // Filling out the SIGCHLD_action struct, Registering handle_SIGCHLD as the signal handler, No flags set
  SIGCHLD_action.sa_handler = handle_SIGCHLD;
  sigfillset(&SIGCHLD_action.sa_mask);
  SIGCHLD_action.sa_flags = 0;
  sigaction(SIGCHLD, &SIGCHLD_action, NULL);
  
  
  // --------------------------------------------------------------------------------------------------
  // loop asking for input until exit entered as the command
  // --------------------------------------------------------------------------------------------------
  
  while (1) {
    // The ignore_action struct as SIG_IGN its signal handler to have SIGINT ignored by whoever will need
    ignore_action.sa_handler = SIG_IGN;
    // Registering the ignore_action as the handler for SIGINT.
    sigaction(SIGINT, &ignore_action, NULL);
    
    
    // --------------------------------------------------------------------------------------------------
    // Prompt for user input, store the input in readBufferInit, check MAX_LEN and MAX_ARG not exceeded
    // --------------------------------------------------------------------------------------------------
   
    // print : as the prompt
    write(1,":",1);

    // clean the buffer, receive an input check the length
    memset(readBufferInit, '\0', sizeof(readBufferInit));

    if (fgets(readBufferInit, MAX_LEN + 2, stdin) == NULL) {
      if (!signalInter) {                                      
        perror("Command input");
        exit(1);
      }
    }

    readLength = strlen(readBufferInit);

    // set to 0 for the next while loop
    signalInter = 0;

    // character +1 is character no 2049,for the '\0', length will be +2 if the input exceeds 2048 character (excl the '\0') 
    if (readLength > MAX_LEN + 1){
      write(2,"You exceeded the command length. Please try again!\n",53); 
      continue;
    }

    // when entered on the command line, the last character ends with \n, while with input redirect, it may not be the case
    if (readBufferInit[readLength-1] == '\n') {
      readBufferInit[readLength-1] = '\0';
      readLength -=1;
    }

    // consecutive blank spaces at the beginning are disregared - searching for the first argument. 
    argSize = 0;
    memset(ch, '\0', sizeof(ch));
    j = 0;
    while (readBufferInit[j] != '\0') {
      if (readBufferInit[j] != ' ') {
        argSize = 1;
        break;
      }
      j++;
    }
    // Continue to the next prompt if no argument found or line starts with "#"
    if (argSize == 0) {
      continue;
    }
    if (readBufferInit[j] == '#') {
      continue;
    }
    // Find number of arguments, return error if it is larger than MAX_ARG
    while (readBufferInit[j] != '\0') {
      ch[0] = readBufferInit[j]; 
      if (strcmp(ch, " ") == 0) {
        ch[0] = readBufferInit[j+1]; 
        if ((strcmp(ch, " ") != 0) && (strcmp(ch, "\0")) != 0) {
          argSize++;
        }
      }
      j++;
    }
    if (argSize > MAX_ARG){
      write(2,"You exceeded the command length. Please try again!\n",53); 
      continue;
    }
    
    // --------------------------------------------------------------------------------------------------
    // Replace $$ in the user input with mainpid and create readBuffer to paste from readBufferInit
    // --------------------------------------------------------------------------------------------------

    // find number of "$$" to be replaced
    int replaceCount = 0;
    char substr[3] = "$$";
    substr[2] = '\0';
    j = 0;
    while (readBufferInit[j+1] != '\0') {
      if ((readBufferInit[j] == substr[0]) && (readBufferInit[j+1] == substr[1])) {
        replaceCount++;
        j++;
      }
      j++;
    }
    
    // create new array for the replacement of "$$", because size of initial array may not be sufficient 
    int newSize = strlen(readBufferInit) + (replaceCount * (strlen(mainpidstr) - 2));
    char readBuffer [newSize+1];
    memset(readBuffer, '\0', sizeof(readBuffer));

    // initiate variables to paste readBufferInit into readBuffer, replacing "$$"s and removing extra spaces
    j = 0;

    // do not paste to the new array the initial space characters " "
    while (readBufferInit[j] == ' ') {j++;}
      
    // Paste remaining - remaining spaces will be taken care of by the strtok later below
    while (readBufferInit[j] != '\0') {
      if ((readBufferInit[j] == substr[0]) && (readBufferInit[j+1] == substr[1])) {
        if (readBuffer == NULL) {
          strcpy(readBuffer, mainpidstr);
        } else {
          strcat(readBuffer, mainpidstr);
        }
        j++;
      } else {
        ch[0] = readBufferInit[j];
        ch[1] = '\0';
        if (readBuffer == NULL) {
          strcpy(readBuffer, ch);
        } else {
          strcat(readBuffer, ch);
        }
      }
      j++;
    }

    // --------------------------------------------------------------------------------------------------
    // Create the arguments array with the size of number of args + 1 (1 additional NULL arg in the end)
    // --------------------------------------------------------------------------------------------------
    
    char *enteredargs[argSize + 1];
    char * token = strtok(readBuffer, " ");
    // parse the first argument (the command) and add to array[0]
    if (token != NULL) {
      enteredargs[0] = (char*)malloc(strlen(token)*sizeof(char));
      strcpy(enteredargs[0],token);
    } 
    // parse the rest arguments
    int i = 1;
    while( token != NULL ) {
      token = strtok(NULL, " ");
      if (token != NULL) {
        enteredargs[i] = (char*)malloc(strlen(token)*sizeof(char));
        strcpy(enteredargs[i],token);
        i++;
      } else {
      // last arg set to NULL
      enteredargs[i] = NULL;
      }
    }

    // --------------------------------------------------------------------------------------------------
    // Cleanup and organize enteredargs for &, < and > (redirection and background commands)
    // --------------------------------------------------------------------------------------------------

    int isBg = 0;
    int hasInpRed = 0;
    char *inputFile;
    int hasOutRed = 0;
    char *outputFile;
    int checkingIndex = argSize - 1;
    
    // if the last non-null argument in the array is &, a seperate variable isBg is set to 1
    if (strcmp(enteredargs[checkingIndex], "&") == 0) {
      // this is a bg process
      isBg = 1;
      // the array is set to null, as it needs to be cleaned for exec() function
      enteredargs[checkingIndex] = NULL;
      checkingIndex--;
    }
    // if there existed & in the end, the last index will reduce by 1
    checkingIndex--;
    // having the & set to null, there are two alternatives; either the last two arguments are related to input 
    // redirection, in this case the previous two may or may not be related to input redirection, or the last or vice versa:

    //1- check if the last arg is input redirection
    if (strcmp(enteredargs[checkingIndex], "<") == 0) {
      // this is input redirection
      hasInpRed = 1;
      inputFile = (char*)malloc(strlen(enteredargs[checkingIndex + 1])*sizeof(char));
      strcpy(inputFile, enteredargs[checkingIndex + 1]);   
      enteredargs[checkingIndex + 1] = NULL;
      enteredargs[checkingIndex] = NULL;
      checkingIndex -= 2;
      // check if there was output redirection before input redirection
      if (strcmp(enteredargs[checkingIndex], ">") == 0) {
        // this is output redirection
        hasOutRed = 1;
        outputFile = (char*)malloc(strlen(enteredargs[checkingIndex + 1])*sizeof(char));
        strcpy(outputFile, enteredargs[checkingIndex + 1]);   
        enteredargs[checkingIndex + 1] = NULL;
        enteredargs[checkingIndex] = NULL;
      }
    }
    //2- check if the last arg is output redirection
    if (strcmp(enteredargs[checkingIndex], ">") == 0) {
      // this is output redirection
      hasOutRed = 1;
      outputFile = (char*)malloc(strlen(enteredargs[checkingIndex + 1])*sizeof(char));
      strcpy(outputFile, enteredargs[checkingIndex + 1]);   
      enteredargs[checkingIndex + 1] = NULL;
      enteredargs[checkingIndex] = NULL;
      checkingIndex -= 2;
      // check if there was input redirection before output redirection
      if (strcmp(enteredargs[checkingIndex], "<") == 0) {
        // this is input redirection
        hasInpRed = 1;
        inputFile = (char*)malloc(strlen(enteredargs[checkingIndex + 1])*sizeof(char));
        strcpy(inputFile, enteredargs[checkingIndex + 1]);   
        enteredargs[checkingIndex + 1] = NULL;
        enteredargs[checkingIndex] = NULL;
      }
    }

    // --------------------------------------------------------------------------------------------------
    // After having organized the args array, process the user input
    // --------------------------------------------------------------------------------------------------
    
    // built in commands
    if (strcmp(enteredargs[0], "cd") == 0) {
      if (enteredargs[1]) {
        if(chdir(enteredargs[1])) {
          perror(enteredargs[1]);
        }
      } else { 
        if(chdir(getenv("HOME"))) {
          perror("HOME directory not defined");
        } 
      }
    } else if (strcmp(enteredargs[0], "exit") == 0) {
      // kill the children
      for(int i=0;i<101;i++){
        if (childPids[i] > 0) {kill(childPids[i], SIGKILL);}
      }
      // terminate the parent
      exit(0);

    } else if (strcmp(enteredargs[0], "status") == 0) {
      if (lastStatusType) {
        printf("terminated by signal %d\n", lastStatus);    
        fflush(stdout);
      } else {
        printf("exit value %d\n", lastStatus);    
        fflush(stdout);
      }
    } else {
      
      // non built in commands
      
      int childStatus;

      // if a background process is entered, check the childPids buffer is not full, otherwise prompt the user to wait
      if ((isBg) && (!fgOnlyMode)) {
        int count = 0;
        for(int i=0; i<101; i++){
          if (childPids[i] == 0 ) {count++;}
        } 
        // to ensured that there is an extra empty spot, after one being used now, 
        // as the child no will move to the next spot later below
        if (count < 2) {
        printf("Cannot start new background process, 100 processes running in the background.\n" 
            "Please wait until a background process ends.\n");
        // disregard the last command, go to the beginning of while look, and reprompt
        continue;
        }
      }
      // fork the child to start the new process
      pid_t spawnPid = fork();
      if(spawnPid== -1) {
        perror("fork()");
        exit(1);
      
      } else if (spawnPid == 0) {
        // in the child :
          
        struct sigaction SIGINT_action = {0}, ignore_action = {0};
        sigaction(SIGINT, &SIGINT_action, NULL);
        
        // block the SIGTSTP
        sigprocmask(SIG_BLOCK, &signal_set_ch, NULL);

        // if running in background process, and if not on foreground only mode, child should ignore SIGINT
        if (isBg && !fgOnlyMode) {
          ignore_action.sa_handler = SIG_IGN;
          sigaction(SIGINT, &ignore_action, NULL);
        }
        
        // set the input / output redirections if requested
        if (hasInpRed) {
          if((fd_i = open(inputFile, O_RDONLY, 00600)) < 0) {
            fprintf(stdout,"cannot open %s for input\n", inputFile);
            exit(1);
          } else {
            if (dup2(fd_i, 0) < 0) {
              perror("Input file dup2"); 
              exit(1);
            }
          }
        }
        if (hasOutRed) {
          if((fd_o = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 00600)) < 0) {
            fprintf(stdout,"cannot open %s for output\n", outputFile);
            exit(1);
          } else {
            if(dup2(fd_o, 1) < 0) {
              perror("Output file dup2"); 
              exit(1);
            }
          }
        }

        // execute non built in commands in the child process
        if (execvp(enteredargs[0], enteredargs)){
          perror(enteredargs[0]);   
          exit(1);          
        }

        // close the file descriptors if opened
        if (hasInpRed) {
          if(close(fd_i) < 0) {
            perror("Input file");
            exit(1);
          }
        }
        if (hasOutRed) {
          if(close(fd_o) < 0) {
            perror("Output file");
            exit(1);
          } 
        }
        
        // exit the child process
        exit(0);
      
      } else {
        // in the parent:

        // block SIGTSTP signal while child process is running - background will not be affected as the parent will run in parallel
        sigprocmask(SIG_BLOCK, &signal_set_ch, NULL);
        
        // check whether bg or fg  if bg, WNOHANG is used
        if (isBg && !fgOnlyMode) {
          
          // this is for the background process - store shild pids in an array for following their status, and further use
          childPids[childNo] = spawnPid;

          // move childNo to the next empty spot in the childPids array - ensured earlier that there is an extra empty spot
          do {
            childNo++;
            if (childNo == 101) {childNo = 0;}     
          } while (childPids[childNo] != 0); 

          // print the pid of the new initiated background process 
          printf("background pid is %d\n",spawnPid);
          fflush(stdout);

          // use NOHANG for background process not to block parent
          spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
        
        } else {
          
          // this is for the foreground process - set status to exit/termination status
          spawnPid = waitpid(spawnPid, &childStatus, 0);

          if (WIFEXITED(childStatus)) {
            lastStatusType = 0;
            lastStatus = WEXITSTATUS(childStatus);
          } else { 
            lastStatus = WTERMSIG(childStatus);
            lastStatusType = 1;
            char lSstr [1];
            sprintf(lSstr, "%d", lastStatus);
            
            write(1,"terminated by signal ",22);
            write(1, lSstr, 1);
            write(1, "\n", 1);
          } 
        }
        
        // unblock SIGTSTP, foreground process stopps here (background child reaches here immediately, parent unaffected from background)
        sigprocmask(SIG_UNBLOCK, &signal_set_ch, NULL);

      } // closing paranthesis for else statement (related to parent process)
    }   // closing paranthesis for else statement (related to non built-in command actions)
  }     // closing paranthesis for while loop
}      
