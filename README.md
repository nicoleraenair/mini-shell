A mini Unix shell program.

Functionality includes:
 - Prompting and parsing user input (into argument list)
 - Built-in shell commands
    - exit: quit shell program
    - history: displays 10 most recently entered commands
    - !num: re-executes previously entered command with id num
 - Forking and reaping child processes to execute commands in the background or foreground
 
Implements circular queue and related functions to store command history and dynamically allocates argument list.
