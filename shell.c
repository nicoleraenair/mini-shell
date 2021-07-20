//This program implements a simple shell program, with functionality for
//in-built commands (exit, history, !num) and running programs in the foreground
//or background.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#include "parsecmd.h"

#define MAXHIST 10   // max number of commands in the history

// used to store history information about past commands
struct history_t {
  char command[MAXLINE];  // the command line from a past command
  unsigned int id;  // unique ID of the command
};

/* global variables: add only globals for history list state
 *                   all other variables should be allocated on the stack
 * static: means these variables are only in scope in this .c file
 *
 * history: a circular list of the most recent command input strings.
 *          Note: there are MAXHIST elements, each element is a
 *          history_t struct, whose definition you can change (by
 *          adding more fields).  It currently has one filed to store
 *          the command line string (a statically declared char array
 *          of size MAXLINE)
 *          Remember: the strcpy function to copy one string to another
 */
static struct history_t history[MAXHIST];
static int history_next=0;
static int history_size=0;
static unsigned int command_id = 0;
static char cmdline_copy[MAXLINE];

//function prototpes
void child_handler(int sig);
void print_queue();
void add_queue(char* cmdline);
int count_args(const char *cmdline);
char **parse_cmd_dynamic(const char *cmdline, int *bg);
int parse_cmd(const char *cmdline, char *argv[], int *bg);
/******************************************************************/
int main( ){

  char cmdline[MAXLINE];
  char *argv[MAXARGS];
  int bg;
  int pid;
  int status;
  int num, i, found;

  signal(SIGCHLD, child_handler);

  while(1) {
    // (1) print the shell prompt (in a cool color!)
    printf("\e[1;36mcs31shell> \e[0m");
    fflush(stdout);

    // (2) read in the next command entered by the user
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
      perror("fgets error");
    }
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    // parses command line
    parse_cmd(cmdline, argv, &bg);
    if(argv[0] == NULL){
        continue;
    }

    if(!strcmp(argv[0], "exit")){
        printf("bye bye\n");
        exit(0);
    }

    //allows user to type ![command ID] to rerun that command
    if(argv[0][0]=='!'){
        num = atoi(&argv[0][1]);
        found = 0;
     // if(num!=0){
            for(i=0; (i< history_size)&&(found==0); i++){
                if(history[i].id == num){
                    strcpy(cmdline, history[i].command);
                    parse_cmd(cmdline, argv, &bg);
                    found = 1;
                }
            }
     // }
        if(found==0){
            printf("%s: not found\n", argv[0]);
            continue;
        }
    }

    add_queue(cmdline);

    if(!strcmp(argv[0], "history")){
         print_queue();
    }
    else if(!bg){
        pid = fork(); // create new process
        if (pid == 0) { /* child */
            if (execvp(argv[0],argv)<0) {
                found = 0;
                printf("%s:Command not found.\n",argv[0]);
                exit(1);
            }
        }
        else { /* parent */
            waitpid(pid,&status,0);
        }
    }
    else if(bg){
        pid = fork(); // create new process
        if(pid == 0) { // child
            if (execvp(argv[0],argv)<0) {
                found = 0;
                printf("%s:Command not found.\n",argv[0]);
                exit(1);
            }
        }
    }

  }
  return 0;
}

/*
 * child_handler - function to reap zombie child processes
 *
 *     sig: the signal
 *     child_status: int indicating whether the child process has completed
 *     pid: child processes' pid
 */
void child_handler(int sig){
    int child_status;
    pid_t pid;
    while((pid = waitpid(-1,&child_status,WNOHANG))>0){ }
}

/*
 * add_queue - adds an entered command-line command to the command history queue
 *    cmdline: the most recently entered command on the command line
 */
void add_queue(char* cmdline){
    strcpy(history[history_next].command, cmdline);
    history[history_next].id = command_id;
    command_id++;

    history_next = (history_next+1)%MAXHIST;

    if(history_size < MAXHIST){
        history_size++;
    }
}
/*
 * print_queue: prints the list of command-line commands in FIFO order
 *     i, j: integers used to parse through the queue of command history and
 *     to print them in FIFO order
 *
 */
void print_queue(){
    int i, j;
    if(history_size < MAXHIST){
        i = 0;
    }
    else{
        i = history_next;
    }
    for(j = 0; j < history_size; j++)
    {
        printf("%d  %s", history[(i + j)%MAXHIST].id, history[(i + j)%MAXHIST].command);
    }
}

int parse_cmd(const char *cmdline, char *argv[], int *bg) {
  int i, in_token, next_token;

  if(cmdline == NULL){
    return -2;
  }
  if(!strcmp(cmdline, "")){
    return -1;
  }
  strncpy(cmdline_copy, cmdline, MAXLINE);

  *bg = 0;
  i = 0;
  in_token = 0;
  next_token = 0;

  while(cmdline_copy[i] != '\0' && next_token < MAXARGS - 1){
    if(cmdline_copy[i] == '&'){
      *bg = 1;
    }
    if(isspace(cmdline_copy[i]) || cmdline_copy[i] == '&'){
      cmdline_copy[i] = '\0';
      in_token = 0;
    }
    else if(!in_token){
      argv[next_token] = &cmdline_copy[i];
      in_token = 1;
      next_token++;
    }
    i++;
  }
  argv[next_token] = NULL;
  return 0;
}


char **parse_cmd_dynamic(const char *cmdline, int *bg) {
  int num_args, in_token, i, token_size, next_arg;
  char **argv;

  i = 0;
  in_token = 0;
  token_size = 0;
  next_arg = 0;

  num_args = count_args(cmdline);

  argv = (char**) malloc(sizeof(char*) * (num_args + 1));
  if(argv == NULL){
    return NULL;
  }

  while(cmdline[i] != '\0'){
    if(cmdline[i] == '&'){
      *bg = 1;
    }
    if((isspace(cmdline[i]) || cmdline[i] == '&')){
      if(in_token){
        in_token = 0;
        argv[next_arg] = (char*) malloc(sizeof(char) * (token_size + 1));
        if(argv[next_arg] == NULL){
          return NULL;
        }
        strncpy(argv[next_arg], &cmdline[i - token_size], token_size);
        argv[next_arg][token_size] = '\0';
        token_size = 0;
        next_arg++;
      }
    }
    else if(!in_token){
      in_token = 1;
      token_size = 1;
    }
    else if(in_token){
      token_size++;
    }
    i++;
  }
  if(token_size > 0){
    argv[next_arg] = malloc(sizeof(char) * (token_size + 1));
    if(argv[next_arg] == NULL){
      return NULL;
    }
    strncpy(argv[next_arg], &cmdline[i - token_size], token_size);
    argv[next_arg][token_size] = '\0';
    next_arg++;
  }
  argv[next_arg] = NULL;

  return argv;
}

int count_args(const char *cmdline){
  int i = 0;
  int num_args = 0;
  int in_token = 0;

  while(cmdline[i] != '\0'){
    if(isspace(cmdline[i]) || cmdline[i] == '&'){
      in_token = 0;
    }
    else if(!in_token){
      in_token = 1;
      num_args++;
    }
    i++;
  }
  return num_args;
}
