#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define MAX_PAIR_NUM 5
#define MAX_COMMANDS 5
#define COMMAND_LENGTH 20

void builtInCommands(char *line, int quit, char history[256][256], int* historyCounter) {

    if (strcmp(line, "quit\n") == 0) {
        printf("%s", line);
        exit(0);
    }

    if (strcmp(line, "history \n") == 0) {
        printf("--------------------------History--------------------------\n");
        for (int i = 0; i < *historyCounter; i++) {
            printf("%s", history[i]);
        }
        printf("----------------------------------------------------------\n");
    }

    if (strncmp(line, "cd ", 3) == 0) { 
        line[strcspn(line, "\n")] = 0; 
        char *dir = line + 3; 

        if (chdir(dir) != 0) {
            perror("chdir"); 
        } else {
            printf("Changed directory to %s\n", dir);
        }
        strcpy(history[*historyCounter], line);

    }


}

void executeCommand(const char *command_with_first_arg, const char *second_arg) {
    char *command_copy = strdup(command_with_first_arg); 
    char *command = strtok(command_copy, " ");
    if (command == NULL) {
        fprintf(stderr, "Invalid command\n");
        free(command_copy);
        return;
    }
    char *first_arg = strtok(NULL, " ");
    if (first_arg == NULL) {
        fprintf(stderr, "Invalid argument\n");
        free(command_copy);
        return;
    }
    if (strcmp(command, "quit") == 0) {
        free(command_copy);
        exit(EXIT_FAILURE);
    }
    execlp(command, command, first_arg, second_arg, NULL);
    perror("execlp");
    free(command_copy);
    exit(EXIT_FAILURE);
}

typedef struct {
    char commands[MAX_COMMANDS][COMMAND_LENGTH];
    int commandCount;
} Command;


void parseCommands(const char *line, Command pairs[MAX_PAIR_NUM], int *pairCounter, int *quit) {
    *pairCounter = 0; 
    char tempLine[256];
    strncpy(tempLine, line, sizeof(tempLine) - 1);
    tempLine[sizeof(tempLine) - 1] = '\0'; 

    char *pairSavePtr; 
    char *commandSavePtr; 
    char *pairPtr = strtok_r(tempLine, "|", &pairSavePtr);

    while (pairPtr != NULL && *pairCounter < MAX_PAIR_NUM) {
        Command* pair = &pairs[(*pairCounter)++];
        pair->commandCount = 0; 

        char *commandPtr = strtok_r(pairPtr, ";", &commandSavePtr);
        while (commandPtr != NULL && pair->commandCount < MAX_COMMANDS) {
        
            strncpy(pair->commands[pair->commandCount], commandPtr, COMMAND_LENGTH - 1);
            pair->commands[pair->commandCount][COMMAND_LENGTH - 1] = '\0';

            if (strstr(commandPtr, "quit") != NULL) {
                *quit = 1; 
            }
            pair->commandCount++; 
            commandPtr = strtok_r(NULL, ";", &commandSavePtr); 
        }

        for (int i = pair->commandCount; i < MAX_COMMANDS; i++) {
            pair->commands[i][0] = '\0'; 
        }

        pairPtr = strtok_r(NULL, "|", &pairSavePtr); 
    }
}

void executeSingle(Command *pair) {
    for (int i = 0; i < pair->commandCount; i++) {
        pid_t pid;
        if ((pid = fork()) == 0) {
            executeCommand(pair->commands[i], NULL);
        }
    }

    while (wait(NULL) > 0); 
}

void executePiping(Command pairs[MAX_PAIR_NUM], int pairCounter) {
    int pipeCatToGrep[2];
    pid_t pid;
    int pids[MAX_COMMANDS];
    int pid_count = 0;

    if (pipe(pipeCatToGrep) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
   
    for (int i = 0 ; i < pairs[0].commandCount; i++) {
        if ((pid = fork()) == 0) {
            close(pipeCatToGrep[0]); 
            dup2(pipeCatToGrep[1], STDOUT_FILENO); 
            close(pipeCatToGrep[1]); 

            executeCommand(pairs[0].commands[i], NULL);
            exit(EXIT_SUCCESS); 
        }
    }

    while (wait(NULL) > 0); 

    close(pipeCatToGrep[1]); 

   
    char array[1024];
    ssize_t count;
    int fileDesc = open("temp_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fileDesc == -1) {
        perror("open temp_file.txt");
        exit(EXIT_FAILURE);
    }

    while ((count = read(pipeCatToGrep[0], array, sizeof(array))) > 0) {
        write(fileDesc, array, count);
    }

    close(pipeCatToGrep[0]); 
    close(fileDesc); 

    
    for (int i = 0; i < pairs[1].commandCount; i++) {
        if ((pid = fork()) == 0) {
            executeCommand(pairs[1].commands[i], "temp_file.txt");
            exit(EXIT_SUCCESS);
        }
    }
    
    while (wait(NULL) > 0); 
    unlink("temp_file.txt"); 
}


int main(int argc, char *argv[]) {
    Command pairs[MAX_PAIR_NUM];
    int pairCounter;
    char line[256];
    int quit = 0;
    char historyArray[256][256];
    int historyCounter = 0;
    
    
    if (argc != 2) {
        return 1;
    }

    FILE *script_file = fopen(argv[1], "r");
    if (script_file == NULL) {
        perror("Error opening script");
        return 1;
    }

    while (fgets(line, sizeof(line), script_file)) {
      
            
            if (strcmp(line, "history \n") == 0 || strcmp(line, "quit\n") == 0 || strncmp(line, "cd ", 3) == 0)
            {
                builtInCommands(line, quit, historyArray, &historyCounter);
            }
            else{
                printf("Executing %s", line);
                parseCommands(line, pairs, &pairCounter, &quit);
                strcpy(historyArray[historyCounter], line);
                historyCounter++;
            
                if (pairCounter == 1) {
                    executeSingle(&pairs[0]);
                }
                else {
                    executePiping(pairs, pairCounter);
            }
        }         
    }
    
    fclose(script_file);
    return 0;
}
