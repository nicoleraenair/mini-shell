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

#define HIST 10
#define MAXLENGTH 1024
#define MAXARGS 128

// used to store history information about past commands
struct history_t {
  char command[MAXLENGTH];  // the command line from a past command
  unsigned int id;  // unique ID of the command
};

static struct history_t history[10]; //history: a circular list of the most 10 recent command input strings
static int history_next=0; //index for next element added to queue
static int history_size=0; //number of elements in queue
static unsigned int command_id = 0; //id to be assigned to next element added to queue
static char cmdline_copy[MAXLENGTH]; //copy of command line for use in parsing function

//function prototpes
void child_handler(int sig);
void print_queue();
void add_queue(char* cmdline);
int count_args(const char *cmdline);
char **parse_cmd_dynamic(const char *cmdline, int *bg);
int parse_cmd(const char *cmdline, char *argv[], int *bg);

int main( ){

  char cmdline[MAXLENGTH];
  char *argv[MAXARGS]; 
  //char** argv;
  int bg;
  int pid;
  int status;
  int num, i, found;

  signal(SIGCHLD, child_handler);

  while(1) {
    printf("\033[1m\033[31m");
    printf("$");
    printf(" \e[0m");
    fflush(stdout);

    fgets(cmdline, MAXLENGTH, stdin);

    parse_cmd(cmdline, argv, &bg);
    //argv = parse_cmd_dynamic(cmdline, &bg);

    if(argv[0] == NULL)
      continue;

    if(!strcmp(argv[0], "exit"))
      exit(0);

    //allows user to type ![command ID] to rerun that command
    if(argv[0][0]=='!'){
      num = atoi(&argv[0][1]);
      found = 0;
      for(i = 0; i < history_size && found == 0; i++){
        if(history[i].id == num){
          strcpy(cmdline, history[i].command);
          parse_cmd(cmdline, argv, &bg);
          //argv = parse_cmd_dynamic(cmdline, &bg);
          found = 1;
        }
      }
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
      pid = fork();
      if (pid == 0) {
        if (execvp(argv[0],argv)<0) {
          found = 0;
          printf("%s:Command not found.\n",argv[0]);
          exit(1);
        }
      }
      else {
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

  history_next = (history_next+1)%10;

  if(history_size < 10){
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
  if(history_size < 10){
    i = 0;
  }
  else{
    i = history_next;
  }
  for(j = 0; j < history_size; j++)
  {
    printf("%d  %s", history[(i + j)%10].id, history[(i + j)%10].command);
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
  strncpy(cmdline_copy, cmdline, MAXLENGTH);

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
