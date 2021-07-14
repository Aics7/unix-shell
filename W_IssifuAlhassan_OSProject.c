/*loads required libraries*/
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

/*function declarations*/
void checkPrl(char *cmdC);
void checkRdir(char *cmdC);
char **parseCmd(char *cmdP);
void execCmd(char *cmdE);
void *execPrlCmd(void *prlCmdIn);
void execExtCmd(char *extCmdIn[]);
void cmdProc(char cmd[1024]);
void stripper(char cmdRow[1024]);

/*global variables*/
char cmd[1024] = "";
char *extCmd = "";
char prlCmds[1024][1024];
char paths[1024][1024];
char error_message[24] = "An error has occurred\n";
volatile int locked = 0;
int numCmd = 0, numArgs = 0, numPrlCmd = 0, numPaths = 2, prl = 0, rDir = 0, rDirErr = 0, pathEmpty = 0, execute = 1;

/*main method*/
int main(int argc, char *argv[])
{
    /*paths*/
    strcat(paths[0],"/bin");
    strcat(paths[1],"/usr/bin");
    /*interactive mode*/
    if(argc == 1)
    {
        while(1)
        {
            printf("%s","wish> ");
            gets(cmd);
            checkPrl(cmd);
            cmdProc(cmd);
        }
    }
    /*batch mode*/
    else if(argc == 2)
    {
        /*Batch file*/
        FILE *batch;
        char batchName[256] = "";
        strcpy(batchName,argv[1]);
        batch = fopen(batchName,"r");
        if (batch == NULL)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        char cmdRow[1024];
        /*gets lines from batch file*/
        while (fgets(cmdRow, sizeof(cmdRow), batch))
        {
            stripper(cmdRow);
            checkPrl(cmdRow);
            cmdProc(cmdRow);
        }
        fclose(batch);
        exit(0);
    }
    else
    {
       write(STDERR_FILENO, error_message, strlen(error_message));
       exit(1);
    }
    return 0;
}

/*checks for parallel commands*/
void checkPrl(char *cmdC)
{
    prl = 0;
    numPrlCmd = 0;
    /*check for '&' sign*/
    if(strchr(cmdC,'&') != NULL)
    {
        prl = 1;
    }
    /*counts '&' signs*/
    if(prl == 1)
    {
        int i;
        for(i = 0; i < strlen(cmdC); i++)
        {
            if(cmdC[i] == '&')
            {
                numPrlCmd++;
            }
        }
        numPrlCmd++;
        char *buffer;
        buffer = strtok(cmdC,"&");
        i = 0;
        while(buffer != NULL)
        {
            memset(prlCmds[i],'\0',sizeof buffer);
            strcat(prlCmds[i],buffer);
            buffer = strtok(NULL, "&");
            i++;
        }
    }
}

/*checks for redirection*/
void checkRdir(char *cmdC)
{
    rDir = 0;
    rDirErr = 0;
    int count = 0;
    /*check for '>' sign*/
    if(strchr(cmdC,'>') != NULL)
    {
        rDir = 1;
    }
    /*counts '>' sign*/
    if(rDir == 1){
        for(int i = 0; i < strlen(cmdC); i++)
        {
            if(cmdC[i] == '>')
            {
                count++;
            }
        }
    }
    /*if more than one '>' sign*/ 
    if (count > 1)
    {
        write(STDERR_FILENO, error_message, strlen(error_message));
        rDir = 0;
        rDirErr = 1;
    }
}

/*parses and returns an array of parsed commands*/
char **parseCmd(char *cmdP)
{
    int length = strlen(cmdP), space_0_tab_1 = 0;
    char *buffer;
    char **retArr = malloc(sizeof(char *) * 15);
    /*commands separated by space?*/
    if(strchr(cmdP,' ') != NULL)
    {
        space_0_tab_1 = 0;
    }
    /*commands separated by tab?*/
    if(strchr(cmdP,'\t') != NULL)
    {
        space_0_tab_1 = 1;
    }
    /*parse using tab*/
    if (space_0_tab_1 == 1)
    {
        buffer = strtok(cmdP,"\t");
        numCmd = 0;
        numArgs = -1;
        while(buffer != NULL)
        {
            retArr[numCmd] = buffer;
            buffer = strtok(NULL, "\t");
            numCmd++;
            numArgs++;
        }
    }
    /*parse using tab*/
    else  
    {
        buffer = strtok(cmdP," ");
        numCmd = 0;
        numArgs = -1;
        while(buffer != NULL)
        {
            retArr[numCmd] = buffer;
            buffer = strtok(NULL, " ");
            numCmd++;
            numArgs++;
        }
    }
    return retArr;
}

/*executes commands*/
void execCmd(char *cmdE)
{
    char **parsed = parseCmd(cmdE);
    char *myArgs[20];
    printf("\a");
    extCmd = parsed[0];
    /*puts command arguments into an array*/
    for (int i = 0; i < numCmd; i++)
    {
        myArgs[i] = parsed[i+1];
    }
    /*exit command*/
    if(strcmp(parsed[0],"exit") == 0)
    {
        if (numCmd == 1)
        {
            exit(0);
        }
        else /*if extra commands are added to exit*/
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
    }
    /*path command*/
    else if (strcmp(parsed[0],"path") == 0) 
    {
        if(numCmd == 1)
        {
            pathEmpty = 1;
        }
        else if (numCmd > 1)
        {
            pathEmpty = 0;
            numPaths = numCmd - 1;
            for (int i = 0; i < numPaths; i++)
            {
                memset(paths[i],'\0',sizeof parsed[i+1]);
                strcat(paths[i],parsed[i+1]);
            }
        }
    }
    /*cd command*/
    else if (strcmp(parsed[0],"cd") == 0) 
    {
        if((numCmd == 1) || (numCmd > 2))
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
        }
        else if (numCmd == 2)
        {
            if(chdir(parsed[1]) != 0)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
    }
    /*command not built-in*/
    else        
    {
        if (pathEmpty == 0)
        {
           execExtCmd(myArgs);
        }
    }
}

/*executes parallel commands*/
void *execPrlCmd(void *prlCmdIn)
{ 
    char *prlCmd = (char *) prlCmdIn;
    /*waits to acquire lock*/
    while(locked == 1){} 
    locked = 1;
    checkRdir(prlCmd);  
    execCmd(prlCmd);
    /*unlock*/
    locked = 0; 
}

/*executes external (not built-in) commands*/
void execExtCmd(char *extCmdIn[])
{
    int rDirFile, found = 0, noSpace = 0;
    char *path;
    char *execArgs[256];
    char rDirFileName[256] = "";
    execArgs[0] = NULL;
    if(rDirErr == 0)
    {
        /*if redirection arguments are more than 1*/
        if(rDir == 1 && numArgs > 1)
        {
            if(strchr(extCmdIn[numArgs-1],'>') != NULL)
            {
                noSpace = 1;
            }
            else
            {
                noSpace = 0;
            }
            /*gets redirection file name*/
            if(strcmp(extCmdIn[numArgs-2], ">") == 0)
            {
                strcpy(rDirFileName,extCmdIn[numArgs-1]);
                numArgs = numArgs - 2;
            }
            else if(noSpace == 1)
            {
                char *buffer;
                buffer = strtok(extCmdIn[numArgs-1],">");
                while(buffer != NULL)
                {
                    strcpy(rDirFileName,buffer);
                    buffer = strtok(NULL, ">");
                }
                numArgs = numArgs - 1;
            }
            else
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
                execute = 0;
            }
        }
        /*if redirection argument is one*/
        else if (rDir == 1 && numArgs == 1)   
        {
            char *buffer;
            buffer = strtok(extCmdIn[numArgs-1],">");
            while(buffer != NULL)
            {
                strcpy(rDirFileName,buffer);
                buffer = strtok(NULL, ">");
            }
            numArgs = numArgs - 1;
        }
        /*creates child process*/
        int ch_p =fork();
        if(ch_p <0)
        {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1); 
        }
        else if (ch_p == 0)     
        {
            /*findss path with command*/
            for (int i = 0; i < numPaths; i++)
            {
                strcpy(path,paths[i]);
                path = strcat(path,"/");
                path = strcat(path,extCmd);
                if(access(path,X_OK) == 0)
                {
                    found = 1;
                    break;
                }
            }
            if(found == 0)
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
            /*path found*/
            if(found == 1 && execute == 1)
            {
                execArgs[0] = path;
                for (int i = 0; i < numArgs; i++)
                {
                    execArgs[i+1] = extCmdIn[i];
                }
                execArgs[numArgs+1] = NULL;
                /*redirection*/
                if (rDir == 1)
                {
                    close(STDOUT_FILENO);
                    rDirFile = open(rDirFileName, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
                    if(rDirFile == -1)
                    {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                    }
                }
                /*execute command*/
                if(execv(path,execArgs)==-1)
                {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(0);
                }
            }
            execute = 1;
        }
        /*wait for child process*/
        else
        {
            int ch_p_wait = wait(NULL);
        }
    }
}

/*procedure for running command*/
void cmdProc(char cmd[1024])
{
    if(prl == 0)
    {
        checkRdir(cmd);
        execCmd(cmd);
    }
    if (prl == 1)
    {
        /*create threads*/
        pthread_t threads[numPrlCmd];
        for(int i = 0; i<numPrlCmd; i++)
        {
            if(pthread_create(&threads[i], NULL, execPrlCmd, prlCmds[i]))
            {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        /*wait for threads*/
        for(int i = 0; i<numPrlCmd; i++)
        {
            pthread_join(threads[i],NULL);
        }
    }
    /*clear parallel flag*/
    prl = 0;
}

/*removes unwanted characters*/
void stripper(char cmdRow[1024])
{
    char *buffer;
    if(strchr(cmdRow,'\n') != NULL)
    {
        buffer = strtok(cmdRow,"\n");
        while(buffer != NULL)
        {
            strcpy(cmdRow,buffer);
            buffer = strtok(NULL, "\n");
        }
    }
    if(strchr(cmdRow,'\r') != NULL)
    {
        buffer = strtok(cmdRow,"\r");
        while(buffer != NULL)
        {
            strcpy(cmdRow,buffer);
            buffer = strtok(NULL, "\r");
        }
    }
    if (strchr(cmdRow,'\0') != NULL)
    {
        buffer = strtok(cmdRow,"\0");
        while(buffer != NULL)
        {
            strcpy(cmdRow,buffer);
            buffer = strtok(NULL, "\0");
        }
    }
    if(strchr(cmdRow,'$') != NULL)
    {
        buffer = strtok(cmdRow,"$");
        while(buffer != NULL)
        {
            strcpy(cmdRow,buffer);
            buffer = strtok(NULL, "$");
        }
    }
}
/*Issifu*/