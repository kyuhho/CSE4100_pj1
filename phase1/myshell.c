/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define PATH_MAX 4096
#define PIPE_MAX 128
/* Function prototypes */
void eval(char *cmdline);
int history_put(char *cmdline);
int builtin_command(char **argv);
int parseline(char *buf, char **argv);
char history_path[PATH_MAX];
int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    getcwd(history_path, sizeof(history_path));
    strcat(history_path, "/.history.txt");
    while (1) {
	/* Read */
	printf("CSE4100-MP-P1> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int q1_flag=0;      // flag for quote '
    int q2_flag=0;      // flag for quote "
   
   // |, &, quote에 대한 예외처리
    int j = 0;
    for (int i = 0; i < strlen(cmdline); i++)
    {
        if(cmdline[i] == '|' || cmdline[i] == '&')
        {
            buf[j++] = ' ';
            buf[j++] = cmdline[i];
            buf[j++] = ' ';
        }
        else if( cmdline[i] == '\"' && (q1_flag == 0 && q2_flag == 0)){// start q2, 삽입 x
            i++;
            buf[j++] = cmdline[i];
            q2_flag=1;
        }
        else if( cmdline[i] == '\'' && (q1_flag == 0 && q2_flag == 0)){// start q1, 삽입 x
            i++;
            buf[j++] = cmdline[i];
            q1_flag=1;
        }
        else if( cmdline[i] == '\"' && q2_flag == 1){//end q2, 삽입 x
            i++;
            buf[j++] = cmdline[i];
            q2_flag=0;
        }
        else if( cmdline[i] == '\'' && q1_flag == 1){//end q1, 삽입 x
            i++;
            buf[j++] = cmdline[i];
            q1_flag=0;
        }
        else if(q1_flag == 1 || q1_flag == 1){
            buf[j++] = cmdline[i];
        }
        else
        {
            buf[j++] = cmdline[i];
        }
    }
    buf[j] = '\0';

    

    bg = parseline(buf, argv);
    if (argv[0] == NULL)  
        return;   /* Ignore empty lines */
    
    history_put(cmdline);

    if (!builtin_command(argv)) {//quit -> exit(0), & -> ignore, other -> run
        if((pid = Fork()) == 0){
            if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al & 
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        if (!bg){ 
            int status;
            Waitpid(pid, &status, 0);
        }
        else//when there is backgrount process!
            printf("%d %s", pid, cmdline);
        
    }
        /* Parent waits for foreground job to terminate */

    
    
    return;
}

int history_put(char *cmdline){// history 입력
    FILE* fp = NULL;
    char buffer[MAXLINE]; //temp buffer
    char last_his[MAXLINE]; //history last line 
    int num; // for !# command exception checking
    int r_flag=0; // if same command rewrited, r_flag=1
    /***history input exception***/
    fp = fopen(history_path,"r"); /*file generate*/
    if(fp != NULL){
        while(fgets(buffer, MAXLINE, fp) != NULL) {//check if same command as last history by r_flag
            strncpy(last_his, buffer, MAXLINE);
        }
        if(strlen(last_his)>0 && !strcmp(last_his, cmdline)) 
                r_flag=1;
        fclose(fp);
    }
    fp = fopen(history_path,"a");
    
    if(!(strncmp(cmdline, "!!",2)==0 || sscanf(cmdline,"!%d",&num) == 1 || r_flag == 1)){// !!, !#, same command input exception
        fputs(cmdline,fp); 
        fclose(fp);
    }
    else fclose(fp);
    return 0;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    int his_num; // for history number !#
    if (!strcmp(argv[0], "quit")) /* quit command */
	    exit(0);
    if (!strcmp(argv[0], "exit")) /* exit command */
	    exit(0);
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    if (!strcmp(argv[0], "cd")){  /*cd command*/
        if(argv[1]==NULL) //cd only
            chdir(getenv("HOME"));
        else if(chdir(argv[1])==-1) //cd command execute
            perror("changing current working directory failed");
        return 1;
    }
    if (!strcmp(argv[0], "history")){  /*history command*/
        FILE* fp = NULL;
        char buffer[MAXLINE];
        int num=1;
        fp = fopen(history_path,"r");
        if(fp == NULL){
            printf("The history is empty\n");
        }
        while (fgets(buffer, MAXLINE, fp) != NULL) {
            printf("%d  %s", num, buffer);
            num++;
        }
        return 1;
    }
    if (strncmp(argv[0], "!!",2)==0){  /*!! command, strncmp는 예외처리를 위함*/
        FILE* fp = NULL;
        fp = fopen(history_path,"r");
        char buffer[MAXLINE];
        char last_his[MAXLINE];
        char new_his[MAXLINE];
        char tmp[MAXLINE]; //!! 뒤에 오는 additional string 저장
        tmp[0] = '\0'; //tmp 초기화
        if (strlen(argv[0])>2){//추가적인 string strcat
            strcpy(tmp,&argv[0][2]);
        }
        while(fgets(buffer, MAXLINE, fp) != NULL) {
            strncpy(last_his, buffer, MAXLINE);
        }
        if(strlen(last_his)>0) {
            last_his[strlen(last_his)-1] = '\0';
            strcat(last_his, tmp);
            last_his[strlen(last_his)] = '\n';
            printf("%s", last_his);
            eval(last_his);
        }
        else {
            printf("The history is empty\n");
        }
        fclose(fp);
        return 1;
    }
    else if (sscanf(argv[0],"!%d",&his_num) == 1){  /*!n command*/
            FILE* fp = NULL;
            fp = fopen(history_path,"r");
            char buffer[MAXLINE];
            char temp[MAXLINE];//addtional string exception
            int search_num=1;
            while(search_num < his_num && fgets(buffer, MAXLINE, fp) != NULL) {
                search_num++;
            }
            if(search_num == his_num && fgets(buffer, 1024, fp) != NULL) {
                if(sscanf(argv[0],"!%d%s",&his_num, temp)==2){// additional string exist
                    
                    buffer[strlen(buffer)-1] = '\0';
                    strcat(temp, "\n");
                    strcat(buffer, temp);
                    printf("%s", buffer);
                    eval(buffer); //re eval
                }
                else{
                    printf("%s", buffer);
                    eval(buffer);
                }
            } 
            else {
                printf("can't fine history number %d\n", his_num);
            }
            fclose(fp);
            return 1;
    }
    else{
            return 0;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */
/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
