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
int back_builtin(char **argv);
int parseline(char *buf, char **argv);
int parseline_return_argc(char *buf, char **argv);
char history_path[PATH_MAX];
/*for phase3*/
typedef struct _job{
    struct _job *link;
    pid_t pid;
    int id;
    char cmd[MAXLINE];
    int state;
    int bg_flag; // job이 foreground (0)인지 background(1)
}job;
job* head;
/*job control functions*/
void add_job(pid_t pid, int state, char *cmdline, int bg);
void print_job();
void delete_job(job *target);
int find_job_lastnum();
job* find_job_by_pid(pid_t d_pid);
pid_t find_job_by_id(int id);
pid_t find_job_fg();
/* Signal handlers */
void SIGINThandler(int sig);
void SIGTSTPhandler(int sig);
void SIGCHLDhandler(int sig);
/*job states*/
#define NONE		0
#define RUNN		1	//running 
#define SUSP		2	//suspended in background
//for sigsuspend
int fg_pid; //for SIGCHLDhandler
int main() 
{
    char cmdline[MAXLINE]; /* Command line */
    getcwd(history_path, sizeof(history_path));
    strcat(history_path, "/.history.txt");
    Signal(SIGINT, SIGINThandler);
    Signal(SIGTSTP, SIGTSTPhandler);
    Signal(SIGCHLD, SIGCHLDhandler);
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

    int pipe_cnt=0; // 파이프 while문 count
    int pipe_flag=0;
    int fd[PIPE_MAX][2];// 나중에 64 -> MAK_COMMANDS
    char *str;
    char tokens[PIPE_MAX][MAXLINE];
    int q1_flag=0; // flag for quote '
    int q2_flag=0; // flag for quote "
    
    

    // 공백문자, quote string exception
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
        else if( cmdline[i] == '\"' && q2_flag == 1){
            i++;
            buf[j++] = cmdline[i];
            q2_flag=0;
        }
        else if( cmdline[i] == '\'' &&  q1_flag == 1){
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

    
    if(strchr(buf, '|') != NULL){
        pipe_flag=1;
        str = strtok(buf,"|");
        while(str != NULL){
            tokens[pipe_cnt][0] = '\0';
            strcpy(tokens[pipe_cnt],str);
            pipe_cnt++;
            str = strtok(NULL,"|");
        }
    }

    if (cmdline[0] == '\n')  
            return;   /* Ignore empty lines */
    history_put(cmdline);

    if(pipe_flag){
        /*pipline 첫번째*/
        pipe(fd[0]); //파이프 생성
        bg = parseline(tokens[0], argv);
        if ((pid = Fork()) == 0) { 
            close(STDOUT_FILENO);
            dup2(fd[0][1], STDOUT_FILENO);
            close(fd[0][1]);
            if(!builtin_command(argv)){//quit -> exit(0), & -> ignore, other -> run    
                if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al & 
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            exit(0);
        }
	    /* Parent waits for foreground job to terminate */
	    if (!bg){ 
	        int status;
            close(fd[0][1]);
            Waitpid(pid, &status, 0);
	    }
	    else//when there is backgrount process!
	        printf("%d %s", pid, cmdline);
        
        /*last pipeline 제외 모두 실행*/
        for (int i = 0; i < pipe_cnt - 1; i++) {
            pipe(fd[i+1]);
            bg = parseline(tokens[i+1], argv);
            if ((pid = Fork()) == 0) { 
                close(STDOUT_FILENO);
                close(STDIN_FILENO);
                dup2(fd[i][0], STDIN_FILENO);
                dup2(fd[i+1][1], STDOUT_FILENO);
                close(fd[i][0]);
                close(fd[i+1][1]);
                if(!builtin_command(argv)){ //quit -> exit(0), & -> ignore, other -> run
                    if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al & 
                        printf("%s: Command not found.\n", argv[0]);
                        exit(0);
                    }
                }
                exit(0);
            }
	        /* Parent waits for foreground job to terminate */
	        if (!bg){ 
	            int status;
                close(fd[i+1][1]);
                Waitpid(pid, &status, 0);
	        }
	        else//when there is backgrount process!
	            printf("%d %s", pid, cmdline);
            
        }
        /** last pipeline**/
        if ((pid = Fork()) == 0) { 
            close(STDIN_FILENO);
            dup2(fd[pipe_cnt-1][0], STDIN_FILENO);
            close(fd[pipe_cnt-1][0]);
            close(fd[pipe_cnt-1][1]);
             if(!builtin_command(argv)){//quit -> exit(0), & -> ignore, other -> run
                if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al & 
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            exit(0);
        }
	    /* Parent waits for foreground job to terminate */
	    if (!bg){ 
	        int status;
            Waitpid(pid, &status, 0);
	    }
	    else//when there is backgrount process!
	        printf("%d %s", pid, cmdline);
        
    }
    else{// 파이프라인 없는 경우
        bg = parseline(buf, argv);
        
        sigset_t mask_all, mask_one, prev_one;
        
        Sigfillset(&mask_all);
        Sigemptyset(&mask_one);
        Sigaddset(&mask_one, SIGCHLD);
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        
        if (!strcmp(argv[0], "cd")){  /*cd command*/
            int ret;
            if(argv[1]==NULL) //cd only
                ret = chdir(getenv("HOME"));
            else if(chdir(argv[1])==-1) //cd command execute
                perror("changing current working directory failed");
            return 1;
        }
        if (!strcmp(argv[0], "quit")) /* quit command */
	        exit(0);
        if (!strcmp(argv[0], "exit")) /* exit command */
            exit(0);
        
        back_builtin(argv);
        if ((pid=Fork())==0) {
            setpgid(0,0);
            if(!builtin_command(argv)){//quit -> exit(0), & -> ignore, other -> run
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);
                if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al & 
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
            exit(0);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, NULL); //add job
        add_job(pid, RUNN, cmdline, bg);
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        /* Parent waits for foreground job to terminate */
        if (!bg){ 
            sigset_t mask, prev; // for SIGCHLD
            Sigemptyset(&mask);
            Sigaddset(&mask, SIGCHLD);
            fg_pid = 1;// foreground flag, wait for SIGCHLD
            while (fg_pid)
                Sigsuspend(&prev);
                /* Optionally unblock SIGCHLD */
            Sigprocmask(SIG_SETMASK, &prev, NULL);
        }
    }
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
	    return 1;
    if (!strcmp(argv[0], "exit")) /* exit command */
	    return 1;
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    if (!strcmp(argv[0], "cd")){  /*cd command*/
        return 1;
    }
    if(!strcmp(argv[0], "jobs")){
        return 1;
    }
    if(!strcmp(argv[0], "kill")){
        return 1;
    }
    if(!strcmp(argv[0], "bg")){
        return 1;    
    }
    if(!strcmp(argv[0], "fg")){
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
    if (sscanf(argv[0],"!%d",&his_num) == 1){  /*!n command*/
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
                    eval(buffer); 
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
int back_builtin(char **argv){ // for background_builtin
    if(!strcmp(argv[0], "jobs")){
        print_job();
        return 1;
    }
    if(!strcmp(argv[0], "kill")){
        int job_id;
        if(argv[1]==NULL){
            printf("Usage : kill %[job_id]\n");
			return 1;
        }
        else if(argv[1][0] != '%') {
			printf("Usage : kill %[job_id]\n");
			return 1;
		}
        else if(sscanf(argv[1],"%%%d",&job_id) == 1){
            pid_t t_pid;
            t_pid= find_job_by_id(job_id);
            if(t_pid == NULL){
                printf("No Such Job\n");
                return 1;
            }
            else{
                Kill(t_pid,SIGKILL);
                return 1;
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "bg")){
        int job_id;
        if(argv[1]==NULL){
            printf("Usage : bg %[job_id]\n");
			return 1;
        }
        else if(argv[1][0] != '%') {
			printf("Usage : bg %[job_id]\n");
			return 1;
		}
        else if(sscanf(argv[1],"%%%d",&job_id) == 1){
            pid_t t_pid;
            job* temp;
            t_pid= find_job_by_id(job_id);
            temp = find_job_by_pid(t_pid);
            if(t_pid == NULL){
                printf("No Such Job\n");
                return 1;
            }
            else if(temp->state == RUNN){
                printf("Already running\n");
                return 1;
            }
            else{
                printf("[%d] running %s\n", temp->id,temp->cmd);
                kill(t_pid, SIGCONT);
                temp->state = RUNN;
                return 1;
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "fg")){
        int job_id;
        if(argv[1]==NULL){
            printf("Usage : fg %[job_id]\n");
			return 1;
        }
        else if(argv[1][0] != '%') {
			printf("Usage : fg %[job_id]\n");
			return 1;
		}
        else if(sscanf(argv[1],"%%%d",&job_id) == 1){
            pid_t t_pid;
            job* temp;
            t_pid= find_job_by_id(job_id);
            temp = find_job_by_pid(t_pid);
            if(t_pid == NULL){
                printf("No Such Job\n");
                return 1;
            }
            else{
                printf("[%d] running %s\n", temp->id,temp->cmd);
                temp->bg_flag =0;
                kill(t_pid, SIGCONT);
                sigset_t mask, prev; // for SIGCHLD
                Sigemptyset(&mask);
                Sigaddset(&mask, SIGCHLD);
                fg_pid = 1;// foreground flag, wait for SIGCHLD
                while (fg_pid)
                    Sigsuspend(&prev);
                    /* Optionally unblock SIGCHLD */
                Sigprocmask(SIG_SETMASK, &prev, NULL);
                return 1;
            }
        }
        return 1;
    }
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
int parseline_return_argc(char *buf, char **argv) 
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

    return argc;
}
/*jobs control functions*/
void add_job(pid_t pid, int state, char *cmdline, int bg){
    char buf[MAXLINE];
    char *argv_temp[MAXARGS];
    int argc=0, i, len = 0;
    argc = parseline_return_argc(cmdline, argv_temp);
    for(i = 0; argv_temp[i] != NULL; i++){ // argv를 buf에 이어붙임
        len += strlen(argv_temp[i]) + 1; 
    }
    len--; 
    if(len >= MAXLINE){
        printf("Error: buffer is too small.\n");
        return;
    }
    buf[0] = '\0';
    for(i = 0; i < (argc - 1); i++){
        strcat(buf, argv_temp[i]);
        strcat(buf, " ");
    }
    strcat(buf, argv_temp[argc - 1]);

    int job_num = find_job_lastnum();
    job* new_job = malloc(sizeof(job));
    new_job->link = NULL;
    new_job->pid = pid;
    new_job->bg_flag = bg;
    strcpy(new_job->cmd, buf);
    new_job->state = state;

    if(head == NULL){
        head = new_job;
        new_job->id = 1;
        return;
    }
    job_num++;
    new_job->id = job_num;
    job* temp = head;
    while(temp->link != NULL){
        temp = temp->link;
    }
    temp->link = new_job;
}

void print_job(){
    if(head == NULL){
        printf("There are no jobs.\n");
        return;
    }
    job* temp = head;
    while(temp != NULL){
        printf("[%d]", temp->id);
        if(temp->state == RUNN){
            printf(" running ");
        } else if(temp->state == SUSP){
            printf(" suspended ");
        } else{
            printf(" unknown state ");
        }
        printf(" %s\n", temp->cmd);
        temp = temp->link;
    }
}
void delete_job(job* target){
    if(head == NULL){
        printf("There are no jobs.\n");
        return;
    }

    if(head == target){
        head = target->link;
        free(target);
        return;
    }

    job* temp = head;
    while(temp->link != target && temp->link != NULL){
        temp = temp->link;
    }

    if(temp->link == NULL){
        printf("Job not found.\n");
        return;
    }

    temp->link = target->link;
    free(target);
}

int find_job_lastnum(){
    job *current = head;
    while(current != NULL && current->link != NULL) {
        current = current->link;
    }
    return current == NULL ? 0 : current->id;
}
job* find_job_by_pid(pid_t d_pid){
    job* current = head;
    while (current != NULL) {
        if (current->pid == d_pid) {
            return current;
        }
        current = current->link;
    }
    return NULL;
}
pid_t find_job_by_id(int id){
    job* current = head;
    while (current != NULL) {
        if (current->id == id) {
            return current->pid;
        }
        current = current->link;
    }
    return NULL;
}
pid_t find_job_fg(){
    job* current = head;
    while (current != NULL) {
        if (current->bg_flag == 0) {
            return current->pid;
        }
        current = current->link;
    }
    return NULL;
}
/* Signal handlers */
void SIGINThandler(int sig){
    pid_t t_pid;
    job* temp;
    t_pid = find_job_fg(); // foreground pid 찾기
    if(t_pid != NULL){
        temp = find_job_by_pid(t_pid); // jobs information 최신화
        Kill(t_pid, SIGKILL); // kill
        printf("\n");
    }
    else{
        rio_writen(STDOUT_FILENO, "\n", 1);
        printf("no foreground process\n");
    }
}
void SIGTSTPhandler(int sig){
    pid_t t_pid;
    job* temp;
    t_pid = find_job_fg(); // foreground pid 찾기
    if(t_pid != NULL){
        temp = find_job_by_pid(t_pid); // jobs information 최신화
        temp->bg_flag = 1;
        temp->state = SUSP;
        Kill(t_pid, SIGTSTP); // kill
        fg_pid = 0; // while 문 탈출
        printf("\n");
    }
    else{
        rio_writen(STDOUT_FILENO, "\n", 1);
        printf("no foreground process\n");
    }
    
}
void SIGCHLDhandler(int sig){
    pid_t pid;
    int status;
    job * temp;
    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);
    // 종료된 모든 자식 프로세스에 대해 상태 정보를 가져옴
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        fg_pid =0;
        temp = find_job_by_pid(pid);
        delete_job(temp); /* Delete child from the job list */
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
}
