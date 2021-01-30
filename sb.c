#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define STACKMAX 512
struct InputObj
{
    bool Background;
    char InputFile[128];
    char OutputFile[128];
    char Command[1028];
    int NumArgs;
    char *Arguments[512];
};


/// NAME: PidStackObj
/// DESC: modified stack for managing background processes
struct PidStackObj
{
    int NumBackPid;
    pid_t BackgroundPids[STACKMAX];
};

//Globals
struct PidStackObj PidStack;
int LastForeGroundStatus;
bool ForegroundOnly = false;
//End Globals


/// NAME: _InitPidObj
/// DESC: Creates pid stack with -1 in each val.
void _InitPidObj()
{
    int i;
    PidStack.NumBackPid = -1;

    for(i = 0; i < STACKMAX; i++){
        PidStack.BackgroundPids[i] = -1;
    }
}


/// NAME: PushBackPid
/// DESC: adds a pid to the stack
void PushBackPid(pid_t processId)
{
    PidStack.BackgroundPids[++(PidStack.NumBackPid)] = processId;
}

/// NAME: RemoveBackPid
/// DESC: removes a pid if an earlier bg process ends.
void RemoveBackPid(pid_t processId)
{
    int i;
    int pidPos;
    for(i = 0; i < PidStack.NumBackPid + 1;i++){
        if(PidStack.BackgroundPids[i] == processId){
            pidPos = i;
            break;
        }
    }

    for(i = pidPos; i < PidStack.NumBackPid + 1;i++){
        PidStack.BackgroundPids[i] = PidStack.BackgroundPids[i+1];
    }
    
    PidStack.NumBackPid--;

}

/// NAME: PopBackPid
/// DESC: removes pid from top
pid_t PopBackPid()
{
    return PidStack.BackgroundPids[PidStack.NumBackPid--];
}

/// NAME: TopBackPid
/// DESC: reads top of stack
pid_t TopBackPid()
{
    return PidStack.BackgroundPids[PidStack.NumBackPid];
}

/// NAME: ChangeDirectorysFromHome
/// DESC: allows user to navigate file directory
int ChangeDirectorysFromHome(char* InputBuffer)
{
    char* HomeDirectoryPath = getenv("HOME"); //gets home path.
    char NewPath[1028];

    InputBuffer[strlen(InputBuffer) -1] = '\0';

    if(strcmp(InputBuffer,"cd") == 0){
        if(chdir(HomeDirectoryPath) != 0){ // cannot find directory
            printf("Directory:%s not found.\n",HomeDirectoryPath);
            return 1;
        }
        return 0;
    }

    memset(NewPath,'\0',sizeof(NewPath));

    strtok(InputBuffer," "); // removing unnessary spacing
    strcpy(InputBuffer,strtok(NULL,""));
    


    //printf("    %s\n",InputBuffer);
    if(InputBuffer[0] == '/'){
        sprintf(NewPath,"%s%s",HomeDirectoryPath,InputBuffer); // goto a specifed directory from home directory
        //printf("Path: %s\n",NewPath);
    }
    else if(strcmp(InputBuffer,"..") == 0){ // go back a folder
        strcpy(NewPath,InputBuffer);
        //printf("Path: %s\n",NewPath);
    }
    else if(strcmp(InputBuffer,"~") == 0){ // go back a folder
        strcpy(NewPath,HomeDirectoryPath);
        //printf("Path: %s\n",NewPath);
    }
    else if(InputBuffer[0] == '.' && InputBuffer[1] == '/'){ // current directory
        sprintf(NewPath,"%s",InputBuffer);
        //printf("Path: %s\n",NewPath);
    }
    else{
        sprintf(NewPath,"%s",InputBuffer); // goto directory from home
        //printf("Path: %s\n",NewPath);
    }
    if(chdir(NewPath) != 0){ // cannot find directory
        printf("Directory:%s not found.\n",NewPath);
        return 1;
    }
    return 0;
}

/// NAME: _IsLeadCharSpecial
/// DESC: Helper function to check for &<> and prevent additonal args
bool _IsLeadCharSpecial(char *str)
{
    bool IsSpecial = false;

    if(str == NULL){ // make sure value isnt null so rest of conditions dont seg fault.
        return true;
    }

    if(str[0] == '&'){ // check for bg.
        IsSpecial = true;
    }
    else if(str[0] == '<'){ // check for input char
        IsSpecial = true;
    }
    else if(str[0] == '>'){ // check for output char
        IsSpecial = true;
    }
    else if(str[0] == '#'){ // check for command char
        IsSpecial = true;
    }

    return IsSpecial;
}

/// NAME: ParseInput
/// DESC: inits struct that holds all args.
void ParseInput(char* InputBuffer,struct InputObj* Obj)
{
    //struct InputObj Obj = malloc(1 * sizeof(struct InputObj));
    char Buffer[1028];
    char *InputFileName;
    char *OutputFileName;
    char *Temp;
    //int i;

    Obj->NumArgs = 0;
    InputBuffer[strlen(InputBuffer) -1] = '\0'; // removed \n

    if(InputBuffer[strlen(InputBuffer) -1] == '&'){ // check for bg enabled
        Obj->Background = true;
        InputBuffer[strlen(InputBuffer) -1] = '\0'; // remove char.
        //printf("Background: enabled\n");
    }
    else{
        Obj->Background = false;
        //printf("Background: disabled\n");
    }

    //command
    memset(Buffer,'\0',sizeof(Buffer)); // clear Buffer
    strcpy(Buffer,InputBuffer); //copy buffer
    strtok(Buffer," "); // grab only command part of input;
    strcpy(Obj->Command,Buffer); // take command place in new obj.
    //printf("Command: %s\n",Obj->Command);

    //InputFile Name
    memset(Buffer,'\0',sizeof(Buffer)); 
    strcpy(Buffer,InputBuffer);
    InputFileName = strstr(Buffer,"<"); // grab everything after <
    if(InputFileName != NULL){
        memmove(InputFileName, InputFileName+2, strlen(InputFileName)); //place everything but "< " in string
        strtok(InputFileName," "); // cut off excess
        InputFileName[strlen(InputFileName)] = '\0'; // add end char
        strcpy(Obj->InputFile,InputFileName);
        //printf("Inputfile is:%s\n",Obj->InputFile);
    }

    //OutputFile Name;
    memset(Buffer,'\0',sizeof(Buffer));
    strcpy(Buffer,InputBuffer);
    OutputFileName = strstr(Buffer,">"); // grab everything after >
    if(OutputFileName != NULL){
        memmove(OutputFileName, OutputFileName+2, strlen(OutputFileName));//place everything but "> " in string
        strtok(OutputFileName," "); // cut off excess
        OutputFileName[strlen(OutputFileName)] = '\0'; // add end char
        strcpy(Obj->OutputFile,OutputFileName);
        //printf("Outputfile is:%s\n",Obj->OutputFile);
    }
     
    //arguments
    memset(Buffer,'\0',sizeof(Buffer));
    strcpy(Buffer,InputBuffer);
    strtok(Buffer," "); // everything before first space

    Temp = strtok(NULL,""); // grab everyhting after first space.

    //printf("Temp Line:%s\n",Temp);
    if(_IsLeadCharSpecial(Temp) == false){ // check if there are any args
        strcpy(Buffer,Temp);
        strtok(Buffer,"<>&#"); // grab everything before args.
        
        strtok(Buffer," "); // cut space
        Obj->Arguments[0] = Buffer; // first arg.
        Obj->NumArgs = 1;
        Temp = strtok(NULL," "); // next arg
        while(Temp != NULL){
            
            Obj->Arguments[Obj->NumArgs] = Temp;// input all args into list.
            Obj->NumArgs++;
            Temp = strtok(NULL," ");
        }
        Obj->Arguments[Obj->NumArgs] = strtok(NULL, ""); // grab last arg.
           
    }
    
    // int i;
    // for(i = 0;i < Obj->NumArgs;i++){
    //     printf("arg%d:%s\n",i,Obj->Arguments[i]);
    // }
}

/// NAME: _InitArgList
/// DESC: creates list of args for execvp
void _InitArgList(struct InputObj* Obj,char** Args)
{
    int i;

    Args[0] = Obj->Command; // first arg is command itself.
    for(i = 0;i < Obj->NumArgs ;i++){
        if(getenv(Obj->Arguments[i]) != NULL){
            Args[i+1] = getenv(Obj->Arguments[i]); // add all args.
        }
        else if(strcmp(Obj->Arguments[i],"$$") == 0){
            sprintf(Args[i+1],"%d",getpid());
        }
        else{
            Args[i+1] = (Obj->Arguments[i]);
        }
    }

    Args[i+1] = NULL; // NEED THIS TO WORK.
}

/// NAME: SetupRedirects
/// DESC: creates redirection of input and output
void SetupRedirects(struct InputObj* Obj)
{
    int InputFileDescriptor = STDIN_FILENO;
    int OutputFileDescriptor = STDOUT_FILENO;

    if(Obj->InputFile[0] != '\0'){ // check if inputs are active.
        //printf("INPUT: %s\n",Obj->InputFile);
        InputFileDescriptor = open(Obj->InputFile,O_RDONLY); // open file.

        if(InputFileDescriptor < 0){ // if not found exit.
            printf("File could not be found.\n");
            exit(1);
        }
        dup2(InputFileDescriptor,0); // change input redirection.
        close(InputFileDescriptor); // close file.
    }
    if(Obj->OutputFile[0] != '\0'){ // check if outputs actve.
        //printf("OUTPUT: %s\n",Obj->OutputFile);
        // Can also use creat() func
        OutputFileDescriptor = open(Obj->OutputFile,O_WRONLY | O_CREAT | O_TRUNC,0644); // create new file or edit.

        if(OutputFileDescriptor < 0){ // check for error.
            printf("Error opening or creating file.");
            exit(1);
        }

        dup2(OutputFileDescriptor,1);//change output directions.
        close(OutputFileDescriptor);
    }
}

/// NAME: RunCommand
/// DESC: creates a child fork for a command.
void RunCommand(struct InputObj* Obj)
{
    pid_t pid = fork();
    char *ArgList[512];
    int ProcessStatus;

    switch(pid)
    {
        case -1: //Error
            printf("Something went wrong with fork().\n");
            exit(1);
            break;

        case 0: //Child
            SetupRedirects(Obj);

            _InitArgList(Obj,ArgList);
            execvp(Obj->Command, ArgList); // run command.
            
            printf("Command not found.\n");
            exit(1);
            break;

        default: // Parent
            if(Obj->Background == true && ForegroundOnly == false){ //setup bg or non bg.
                PushBackPid(pid); // add to bg stack.
                printf("Background Pid is %d\n",TopBackPid());
            }
            else{
                
                waitpid(pid,&ProcessStatus,0); // hang the shell is bg inactive.
                LastForeGroundStatus = ProcessStatus;
                //printf("parent(%d) waited for child process(%d)\n",getpid() ,pid);
            }
            break;
    }
}

/// NAME: TrapStopSignal
/// DESC: signal handler for ^z
//SOURCE: https://piazza.com/class/ixhzh3rn2la6vk?cid=365
void TrapStopSignal(int sig)
{
    if(ForegroundOnly == false){
        char* message = ("\nEntering foreground-only mode (& is now ignored)\n"); // enable Fg mode.
        write(STDOUT_FILENO, message, 50);
        ForegroundOnly = true; // change global.
    }
    else{
        char* message = "\nExiting foreground-only mode\n"; // exit Fg mode.
        write(STDOUT_FILENO, message, 31);
        ForegroundOnly = false; // change gloabl.
    }
}

/// NAME: TrapChildSignal
/// DESC: signal handler for a child process ending.
/// I cannot use a function to check what signal is which else TrapChildSignal is exited.
/// SOURCE: http://stackoverflow.com/questions/2377811/tracking-the-death-of-a-child-process
void TrapChildSignal(int sig)
{
    //THIS: http://carpediem101.com/wp-content/uploads/it-works-why.jpg

    pid_t ChildPid;
    int ChildStatus;
    int i;

    //while( (ChildPid = waitpid(-1,&ChildStatus,WNOHANG)) != -1 ){
    for(i = 0;i < PidStack.NumBackPid + 1;i++){ // find pid that exited.
        ChildPid = waitpid(PidStack.BackgroundPids[i],&ChildStatus,WNOHANG);

        if((ChildStatus == 0 || ChildStatus == 1) && ChildPid != 0 ){ // if exited or errored on exit.
            fprintf(stdout,"\nBackground pid %d is done: exit value %d\n",ChildPid,ChildStatus);
            RemoveBackPid(ChildPid);
            //PopBackPid();
        }
        else if(ChildPid != 0){ // other signals.
            fprintf(stdout,"\nBackground pid %d is done: terminated by signal %d\n", ChildPid, ChildStatus);
            RemoveBackPid(ChildPid);
            //PopBackPid();
        }
    }
    
}

/// NAME: TrapChildSignal
/// DESC: signal handler for ^c of a fg process.
void TrapTermSignal(int sig)
{
    printf("\nterminated by signal %d\n",sig); 
}

/// NAME: FreeAndClearInputObj
/// DESC: Helper thats frees input object.
void FreeAndClearInputObj(struct InputObj* Obj)
{
    //int i;

    Obj->Background = false; // reset bg
    //clear all fields.
    memset(Obj->InputFile,'\0',sizeof(Obj->InputFile));
    memset(Obj->OutputFile,'\0',sizeof(Obj->OutputFile));
    memset(Obj->Command,'\0',sizeof(Obj->Command));

    // for(i = 0; i < Obj->NumArgs;i++){
    //     memset(Obj->Arguments[i],'\0',sizeof(Obj->Arguments[i]) );
    // }

    free(Obj);//IMPORTANT WITHOUT THIS SIGABRT SIGNAL WILL BE TRIGGERED <<<<<<<<
}

/// NAME: KillBGProcesses
/// DESC: helper function for exting.
void KillBGProcesses()
{
    int i;
    for(i = 0;i < PidStack.NumBackPid + 1;i++){
        kill(PidStack.BackgroundPids[i], SIGINT); // interrupt all bg pids.
    }
}

/// NAME: CheckForBGMode
/// DESC: switches fg modes if a program returns a stop signals.
void CheckForBGMode()
{
    if(WTERMSIG(LastForeGroundStatus) == 11 && ForegroundOnly == true){ // if signal is stop and fg true switch fg moce.
        printf("\nExiting foreground-only mode\n");
        ForegroundOnly = false;
    }
    else if(WTERMSIG(LastForeGroundStatus) == 11 && ForegroundOnly == false){ // if signal is stop and fg false switch fg mode.
        printf("\nEntering foreground-only mode (& is now ignored)\n");
        ForegroundOnly = true;
    }
}

/// NAME: RunShell
/// DESC: runs the shell itself.
void RunShell()
{
    char InputBuffer[1028];
    struct InputObj *Obj;
    int ForegroundStatus;

    //init signals
    struct sigaction StopSignal;
    StopSignal.sa_handler = TrapStopSignal;
    StopSignal.sa_flags = 0;

    struct sigaction TermSignal;
    TermSignal.sa_handler = TrapTermSignal;
    StopSignal.sa_flags = 0;

    struct sigaction ChildSignal;
    ChildSignal.sa_handler = TrapChildSignal;
    StopSignal.sa_flags = 0;
    //end init signals

    do
    {
        //reseting signal handlers.
        sigaction(SIGTSTP,&StopSignal, NULL);
        sigaction(SIGINT,&TermSignal, NULL);
        sigaction(SIGCHLD,&ChildSignal, NULL);

        // printf("STATUS: %d FGMODE:%d\n",WTERMSIG(LastForeGroundStatus),ForegroundOnly);
        CheckForBGMode();

        //clearing stdin and out.
        fflush(stdout);
        fflush(stdin);

        printf(": ");
        memset(InputBuffer,'\0',sizeof(InputBuffer));
        fgets(InputBuffer,sizeof(InputBuffer),stdin); // get command line.
        //getline(&InputBuffer,&LineMax,stdin);
        
        fflush(stdout);
        fflush(stdin);

        if(strncmp(InputBuffer,"exit",4) == 0){ // exit shell.
            //printf("Exiting....\n");
            KillBGProcesses();
            exit(0);
        }
        else if(strncmp(InputBuffer, "#",1) == 0){ // comment.
            continue;
        }
        else if(strncmp(InputBuffer,"cd", 2) == 0){ // change directory
            //printf("Changing directory...\n");
            ChangeDirectorysFromHome(InputBuffer);
        }
        else if(strncmp(InputBuffer,"status",6) == 0){ // check last fg command status.
            if(WEXITSTATUS(LastForeGroundStatus)){
                ForegroundStatus = WEXITSTATUS(LastForeGroundStatus); // check if extied.
            }
            else{
                ForegroundStatus = WTERMSIG(LastForeGroundStatus); // check if terminated by signal.
            }
            printf("exit value %d\n",ForegroundStatus);
        }
        else{
            if(InputBuffer != NULL && strcmp(InputBuffer,"") != 0){
                //read in a command.
                Obj = malloc(1 * sizeof(struct InputObj)); 
                ParseInput(InputBuffer,Obj); // parse command line.
                RunCommand(Obj); // run ocmmand.

                FreeAndClearInputObj(Obj);
            }
            else{
                continue;
            }
        }
    }
    while(true);
}

/// NAME: main
/// DESC: RUNS THIS THING.
int main(void)
{
    _InitPidObj();
    RunShell(); // singleton for shell.
    return 0;
}
