#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <debug.h>
#include "imprimer.h"

#define MAXLIST     2000
#define NAME_SIZE   20

#define ERR         0
#define PRINTERS    1
#define JOBS        2
#define HELP        3

/*
 * "Imprimer" printer spooler.
 */

typedef struct type_node{
    char name[MAXLIST];
    int id;
    struct type_node* next;
}TYPE_NODE;

typedef struct job_node{
    JOB* a_job;
    struct job_node* next;
}JOB_NODE;

typedef struct convert_node{
    char* type1;
    char* type2;
    char* name;
    char* arg[MAXLIST];
    struct convert_node* next;
    struct convert_node* path_next;
}CONVERT;


FILE* fp_in=NULL, *fp_out=NULL;
TYPE_NODE type_head;
JOB_NODE job_head;
CONVERT convert_head, convert_path;
int imp_num_printers=0, imp_num_jobs=0, imp_num_types=0;
PRINTER printers[MAX_PRINTERS];
PRINTER_SET all_printer;
char out_put[MAXLIST];
int matrix[MAXLIST][MAXLIST];

void complete_handler(int SIGNUM);
void pause_handler(int SIGNUM);
void resume_handler(int SIGNUM);
void remove_completed_jobs();
void print_message(int type, char* str);
int take_input(char* str);
int parse_space(char* str, char** parsed);
int parse_dot(char* str, char** parsed);
void print_help();
void add_type(char* type_name);
int is_type(char* type_name);
int get_type_id(char* type_name);
char* get_type_name(int id);
int create_printer(char* printer_name, char* type_name);
int add_printer_2job(char* printer_name, PRINTER_SET* set);
int create_job(char* file_name, char** arguments, int n);
void* get_conversion_by_type(int first, int second);
void create_conversion(char* type1, char* type2, char* program, char** arg, int n);
int search_conversion_path(int from, int to, int* path, int curr, int* visited);
int start_job(int printer_id, JOB* job, int* path, int size);
int search_Qd_job(int printer_id);
int enable_printer(char* name);
int disable_printer(char* name);
void* get_job_byid(char* id);
int cancel_job(char* id);
int pause_job(char* id);
int resume_job(char* id);
void show_matrix();
void process_arguments(char** arguments, int size);




int main(int argc, char *argv[])
{
    char optval;
    while(optind < argc) {
    	if((optval = getopt(argc, argv, "i:o:")) != -1) {
    	    switch(optval) {
                case 'i':
                    fp_in=fopen(optarg,"r");
                    break;
                case 'o':
                    fp_out=fopen(optarg,"w+");
                    break;
        	    case '?':
        		fprintf(stderr, "Usage: %s [-i <cmd_file>] [-o <out_file>]\n", argv[0]);
        		exit(EXIT_FAILURE);
        		break;
        	    default:
        		break;
        	}
	   }
    }

    while(1){
        char input[MAXLIST], *arguments[MAXLIST];

        //if nothing input
        if(take_input(input))
            continue;

        if(fp_out)
            fprintf(fp_out, "imp> %s\n", input);

        int size=parse_space(input,arguments);

        remove_completed_jobs();

        process_arguments(arguments, size);

    }

    exit(EXIT_SUCCESS);
}


void process_arguments(char** arguments, int size){

    if(strcmp(arguments[0],"quit")==0 && size==1){
        if(fp_out)
            fclose(fp_out);
        exit(1);
    }
    else if(strcmp(arguments[0],"help")==0 && size==1)
        print_message(HELP,NULL);

    else if(strcmp(arguments[0],"type")==0 && size==2)
        add_type(arguments[1]);

    else if(strcmp(arguments[0],"printer")==0 && size==3){
        if(!create_printer(arguments[1], arguments[2]))
            print_message(ERR,"set printer failed");
    }
    else if(strcmp(arguments[0],"printers")==0 && size==1){
        print_message(PRINTERS, NULL);

    }else if(strcmp(arguments[0],"jobs")==0 && size==1){
        print_message(JOBS, NULL);
    }
    else if(strcmp(arguments[0],"print")==0 && size>=2){
        signal(SIGCHLD, complete_handler);
        if(!create_job(arguments[1],arguments, size))
            print_message(ERR, "create job failed");

    }else if(strcmp(arguments[0],"conversion")==0 && size>=4){
        char** arg;
        for(int i=4;i<size;i++)
            strcpy(*arg++,arguments[i]);
        create_conversion(arguments[1],arguments[2],arguments[3],arg, size-4);
    }
    else if(strcmp(arguments[0],"enable")==0 && size==2){
        signal(SIGCHLD, complete_handler);
        if(!enable_printer(arguments[1]))
            print_message(ERR, "enable printer failed");
    }
    else if(strcmp(arguments[0],"disable")==0 && size==2){
        if(!disable_printer(arguments[1]))
            print_message(ERR, "disable printer failed");
    }
    else if(strcmp(arguments[0],"cancel")==0 && size==2){
        if(!cancel_job(arguments[1]))
            print_message(ERR, "cancel job failed");
    }
    else if(strcmp(arguments[0],"pause")==0 && size==2){
        signal(SIGCHLD, pause_handler);
        if(!pause_job(arguments[1]))
            print_message(ERR, "pause job failed");
    }
    else if(strcmp(arguments[0],"resume")==0 && size==2){
        signal(SIGCHLD, resume_handler);
        if(!resume_job(arguments[1]))
            print_message(ERR, "resume job failed");
    }
    else{
        print_message(ERR, "wrong command");
    }

}

int take_input(char* str){
    char* buf;

    if(fp_in){
        if(fgets(str,MAXLIST,fp_in)!=NULL){
            if(str[0]==13 && str[1]==10){
                printf("imp> \n");
                return 1;
            }
            while(str[strlen(str)-1]<48)
                str[strlen(str)-1]='\0';
            printf("imp> %s\n", str);
        }
        else{
            fclose(fp_in);
            fp_in=NULL;
            return 1;
        }
        return 0;
    }
    buf = readline("imp> ");
    if (strlen(buf) != 0) {
        add_history(buf);
        strcpy(str, buf);

        return 0;
    } else {
        return 1;
    }
}

int parse_space(char* str, char** parsed){
    char *token;
    token=strtok(str," ");
    int i=0;
    while(token!=NULL){
        parsed[i++]=token;
        token=strtok(NULL, " ");
    }
    return i;
}

int parse_dot(char* str, char** parsed){
    char *token;
    token=strtok(str,".");
    int i=0;
    while(token!=NULL){
        parsed[i++]=token;
        token=strtok(NULL, ".");
    }
    return i;
}

void add_type(char* type_name){
    if(is_type(type_name)){
        print_message(ERR, "type already exists");
        return;
    }

    TYPE_NODE* temp=(TYPE_NODE*)malloc(sizeof(TYPE_NODE));
    strcpy(temp->name,type_name);
    temp->name[strlen(type_name)]='\0';
    temp->id=imp_num_types++;

    TYPE_NODE* curr=&type_head;
    while(curr->next!=NULL){
        curr=curr->next;
    }

    curr->next=temp;

}

int is_type(char* type_name){
    for(TYPE_NODE* curr=type_head.next; curr!=NULL; curr=curr->next){
        if(strcmp(curr->name,type_name)==0)
            return 1;
    }
    return 0;
}

int create_printer(char* printer_name, char* type_name){
    if(!is_type(type_name)){
        print_message(ERR, "no such type");
        return 0;
    }

    //check if number of printers exceeds max printer num
    if(imp_num_printers>MAX_PRINTERS){
        print_message(ERR, "no more place for new printers");
        return 0;
    }

    PRINTER printer;
    //create new printer
    PRINTER* p=&printer;
    p->name=(char*)malloc(NAME_SIZE*sizeof(char));
    p->type=(char*)malloc(NAME_SIZE*sizeof(char));
    p->enabled=0;
    p->busy=0;


    p->name=strdup(printer_name);
    p->type=strdup(type_name);
    p->id=imp_num_printers;

    printers[imp_num_printers++]=*p;

    return 1;
}

int add_printer_2job(char* printer_name, PRINTER_SET* set){
    for(int i=0;i<imp_num_printers;i++){
        if(strcmp(printers[i].name,printer_name)==0){
            int id=printers[i].id;
            *set|=(1<<id);
            return 1;
        }
    }
    print_message(ERR, "no such printer");
    return 0;
}

int create_job(char* file_name, char** arguments, int n){
    JOB* temp_job=malloc(sizeof(JOB));

    temp_job->file_name=strdup(file_name);
    char* parsed[MAXLIST];
    int size=parse_dot(file_name, parsed);

    //check if file name is correct
    if(size!=2){
        print_message(ERR, "file name incorrect");
        return 0;
    }

    temp_job->file_type=strdup(parsed[1]);

    //job id
    temp_job->jobid=imp_num_jobs++;

    //job time
    struct timeval* creation_time=&temp_job->creation_time;
    struct timeval* change_time=&temp_job->change_time;
    if(gettimeofday(creation_time, NULL)!=0){
        print_message(ERR, "set time failed");
        return 0;
    }
    if(gettimeofday(change_time,NULL)!=0){
        print_message(ERR, "set c time faled");
        return 0;
    }

    //job pid
    temp_job->pgid=0;

    //job eligible printers
    PRINTER_SET temp_set=0;
    for(int i=2;i<n;i++){
        if(!add_printer_2job(arguments[i],&temp_set)){
            return 0;
        }
    }

    temp_job->eligible_printers=temp_set;
    if(temp_set==0)
        temp_job->eligible_printers=ANY_PRINTER;
    temp_job->status=QUEUED;

    //set job to list
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){
        curr=curr->next;
    }
    JOB_NODE* temp=malloc(sizeof(JOB_NODE));
    temp->a_job=temp_job;
    curr->next=temp;
    printf("%s\n", imp_format_job_status((curr->next->a_job),out_put,MAXLIST));
    if(fp_out)
        fprintf(fp_out,"%s\n", imp_format_job_status((curr->next->a_job),out_put,MAXLIST));


    //check if eligible printers are enabled and not busy
    for(int i=0;i<imp_num_printers;i++){
        if((temp_job->eligible_printers & (1<<i))!=0 && printers[i].enabled && printers[i].busy==0){

            int path[imp_num_types];
            int visited[imp_num_types];
            for(int i=0;i<imp_num_types;i++){
                path[i]=0;
                visited[i]=0;
            }

            int from=get_type_id(temp_job->file_type);
            int to=get_type_id(printers[i].type);

            //let the printer start print this job
            if(!search_conversion_path(from, to, path, 0, visited)){
                continue;
            }else{
                // fork a master process for the job and start printing
            sigset_t mask_all, prev_all;
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            int size=0;
            for(int i=0;path[i]!=to;i++){
                size++;
            }

            if(!start_job(i, curr->a_job, path, size+1))
                return 0;

            sigprocmask(SIG_SETMASK, &prev_all, NULL);
            }
        }

    }


    return  1;
}

void print_message(int type, char* str){

    if(type==PRINTERS){
        for(int i=0;i<imp_num_printers;i++){
            printf("%s\n", imp_format_printer_status(&printers[i],out_put,MAXLIST));
            if(fp_out) fprintf(fp_out, "%s\n", imp_format_printer_status(&printers[i],out_put,MAXLIST));
        }
    }else if(type==ERR){
        printf("%s\n", imp_format_error_message(str,out_put,MAXLIST));
        if(fp_out) fprintf(fp_out,"%s\n", imp_format_error_message(str,out_put,MAXLIST));

    }else if(type==JOBS){
        JOB_NODE* curr=job_head.next;
        while(curr!=NULL){
            printf("%s\n", imp_format_job_status((curr->a_job),out_put,MAXLIST));
            if(fp_out)
                fprintf(fp_out, "%s\n", imp_format_job_status((curr->a_job),out_put,MAXLIST));
            curr=curr->next;
        }
    }else if(type==HELP){
        printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", "Welcome to my shell\n",
            "Available commands list below\n",
            "Miscellaneous commands:\n",
            "\thelp (no arguments)\n",
            "\tquit (no arguments)\n",
            "Configuration commands\n",
            "\ttype file_type\n",
            "\tprinter printer_name file_type\n",
            "\tconversion file_type1 file_type2 conversion_program [arg1 arg2 ...]\n",
            "tnfomation commands\n",
            "\tprinters\n",
            "\tjob\n",
            "Spooling Commands\n",
            "\tprint file_name [printer1 printer2 ...]\n",
            "\tcancel job_number\n",
            "\tpause job_number\n",
            "\tresume job_number\n",
            "\tdisable printer_name\n",
            "\tenable printer_name\n");
        if(fp_out) fprintf(fp_out,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", "Welcome to my shell\n",
            "Available commands list below\n",
            "Miscellaneous commands:\n",
            "\thelp (no arguments)\n",
            "\tquit (no arguments)\n",
            "Configuration commands\n",
            "\ttype file_type\n",
            "\tprinter printer_name file_type\n",
            "\tconversion file_type1 file_type2 conversion_program [arg1 arg2 ...]\n",
            "tnfomation commands\n",
            "\tprinters\n",
            "\tjob\n",
            "Spooling Commands\n",
            "\tprint file_name [printer1 printer2 ...]\n",
            "\tcancel job_number\n",
            "\tpause job_number\n",
            "\tresume job_number\n",
            "\tdisable printer_name\n",
            "\tenable printer_name\n");
    }
}

void create_conversion(char* type1, char* type2, char* program, char** arg, int n){
    if(is_type(type1)==0 || is_type(type2)==0){
        print_message(ERR, "no such type");
        return;
    }
    if(strcmp(type1, type2)==0)
        return;

    //add conversion to matrix
    int first=get_type_id(type1);
    int second=get_type_id(type2);
    matrix[first][second]=1;

    CONVERT* temp=malloc(sizeof(CONVERT));
    temp->type1=strdup(type1);
    temp->type2=strdup(type2);
    temp->name=strdup(program);
    temp->arg[0]=strdup(program);
    for(int i=1;i<n;i++){
        temp->arg[i]=strdup(program);
    }

    temp->next=NULL;

    // search for type1 type2 already exist
    CONVERT* curr=&convert_head;
    while(curr->next!=NULL){
        if(strcmp(curr->next->type1, type1)==0 && strcmp(curr->next->type1, type1)==0){
            temp->next=curr->next->next;
            curr->next=temp;;
            return;
        }
        curr=curr->next;
    }
    curr->next=temp;
    return;
}

int enable_printer(char* name){
    // check printer name
    int FOUND=0;
    int  i;
    for(i=0;i<imp_num_printers;i++){
        if(strcmp(printers[i].name,name)==0){
            FOUND=1;
            break;
        }
    }
    if(!FOUND){
        print_message(ERR, "no such printer");
        return 0;
    }

    printers[i].enabled=1;
    search_Qd_job(i);
    return 1;
}


int search_Qd_job(int printer_id){
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){

        if(printers[printer_id].enabled==0)
            break;

        curr=curr->next;

        PRINTER_SET temp=curr->a_job->eligible_printers;
        if((temp & (1 << printer_id))!=0 && curr->a_job->status==QUEUED){


            //found a job
            char* printer_type=strdup(printers[printer_id].type);
            char* job_type=strdup(curr->a_job->file_type);


            //search for conversion path
            int from=get_type_id(job_type);
            int to=get_type_id(printer_type);

            int path[imp_num_types];
            int visited[imp_num_types];
            for(int i=0;i<imp_num_types;i++){
                path[i]=0;
                visited[i]=0;
            }

            if(!search_conversion_path(from, to, path, 0, visited))
                continue;


            sigset_t mask_all, prev_all;
            sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            int size=0;
            for(int i=0;path[i]!=to;i++){
                size++;
            }

            if(!start_job(printer_id, curr->a_job, path, size+1))
                return 0;

            sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
    }
    return 1;
}

int search_conversion_path(int from, int to, int* path, int curr, int* visited){
    path[curr]=from;
    if(from==to){
        return 1;
    }
    for(int i=0;i<imp_num_types;i++){
        if(matrix[from][i]==1 && visited[i]==0){
            visited[i]=1;
            if(search_conversion_path(i,to,path, curr+1, visited))
                return 1;
        }
    }
    return 0;
}

void show_matrix(){
    for(int i=0;i<imp_num_types;i++){
        for(int j=0;j<imp_num_types;j++)
            printf("%d ", matrix[i][j]);
        printf("\n");
    }
}

int start_job(int printer_id, JOB* job, int* path, int size){

    pid_t master_id;

    if((master_id=fork())==0){
        //master process start
        setpgid(0, master_id);

        //wait until the printer is not busy
        while(1)
            if(printers[printer_id].busy==0)
                break;


        //re arrange path to a new path without repetition
        int npath[size], nsize=size, index=0;
        for(int i=0;i<size;i++){
            npath[index++]=path[i];
            for(int j=i+1;j<size;j++){
                if(path[i]==path[j]){
                    i=j;
                    nsize-=j;
                    break;
                }
            }
        }


        //open file in read side
        int in=open(job->file_name,O_RDONLY);
        int fd[nsize][2];
        int i;
        debug("%d ",nsize);
        for(i=0;i<nsize;i++){

            if(nsize==1){
                dup2(in,0);
                break;
            }

            pipe(fd[i]);
            if(fork()==0){
                setpgid(0, master_id);

                if(i==0){
                    dup2(in,0);
                    close(in);

                    dup2(fd[i][1],1);

                    char* a[]={"/bin/cat",NULL};

                    if(execv("/bin/cat", a)){
                        debug("%d", errno);
                        debug("failed conversion");
                    }
                    exit(0);
                }
                else{
                    close(fd[i-1][1]);
                    dup2(fd[i-1][0],0);
                    close(fd[i-1][0]);
                    close(fd[i][0]);

                    dup2(fd[i][1],1);

                    CONVERT* curr=get_conversion_by_type(npath[i-1], npath[i]);

                    if(execvp(curr->name, curr->arg)){
                        debug("%d", errno);
                        debug("failed conversion");
                    }
                    exit(0);
                }
            }

            close(fd[i][1]);
            if(i!=0)
                close(fd[i-1][0]);
        }


        if(fork()==0){
            setpgid(0, master_id);

            if(nsize!=1){
                dup2(fd[nsize-1][0],0);
                close(fd[nsize-1][0]);
                close(fd[nsize-1][1]);
            }

            int out=imp_connect_to_printer(&printers[printer_id], PRINTER_NORMAL);
            dup2(out,1);
            //close(out);

            char* arg[]={"/bin/cat",NULL};
            execv("/bin/cat", arg);
            exit(0);
        }

        pid_t wpid;
        while ((wpid = waitpid(-1, NULL, 0)) > 0){
            debug("master id: %d! reaper_child: %d\n", getpid(), wpid);
        }
        exit(0);
    }
    //main process
    job->pgid=master_id;


    job->status=RUNNING;
    job->chosen_printer=&printers[printer_id];
    job->chosen_printer->busy=1;
    struct timeval* change_time=&job->change_time;
    print_message(JOBS,NULL);
    if(gettimeofday(change_time,NULL)!=0){
        print_message(ERR, "set c time faled");
        return 0;
    }

    return 1;
}


void* get_conversion_by_type(int first, int second){
    CONVERT* curr=&convert_head;
    while(curr->next!=NULL){
        curr=curr->next;
        char* a=curr->type1;
        char* b=curr->type2;
        if(first==get_type_id(a) && second==get_type_id(b)){
            return curr;
        }
    }
    return NULL;
}

int get_type_id(char* type_name){
    TYPE_NODE* curr=&type_head;
    while(curr->next!=NULL){
        curr=curr->next;
        if(strcmp(type_name,curr->name)==0)
            return curr->id;
    }
    return -1;
}

char* get_type_name(int id){
    TYPE_NODE* curr=&type_head;
    while(curr->next!=NULL){
        curr=curr->next;
        if(curr->id==id)
            return curr->name;
    }
    return NULL;
}

int disable_printer(char* name){
    int FOUND=0;
    int  i;
    for(i=0;i<imp_num_printers;i++){
        if(strcmp(printers[i].name,name)==0){
            FOUND=1;
            break;
        }
    }
    if(!FOUND){
        print_message(ERR, "no such printer");
        return 0;
    }

    printers[i].enabled=0;

    return 1;
}


void* get_job_byid(char* id){
    char *token;
    char parsed[MAXLIST];
    token=strtok(id,"");
    int i=0;
    while(token!=NULL){
        parsed[i++]=*token;
        token=strtok(NULL, "");
    }
    int a=0;
    for(int j=0;j<i;j++){
        if(parsed[j]>57 || parsed[j]<48){
            print_message(ERR, "invalid job id");
            return NULL;
        }
        else{
            a=a*10+parsed[j]-48;
        }
    }
    debug("job id %d", a);
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){
        curr=curr->next;
        if(curr->a_job->jobid==a)
            return curr->a_job;
    }

    return NULL;
}



int cancel_job(char* id){
    JOB* job=get_job_byid(id);
    if(job==NULL)
        return 0;

    debug("job %d %d", job->jobid, job->pgid);
    killpg(job->pgid, SIGTERM);
    job->status=ABORTED;
    gettimeofday(&job->change_time, NULL);
    printf("%s\n",imp_format_job_status(job, out_put, MAXLIST));
    if(fp_out)
        fprintf(fp_out, "%s\n",imp_format_job_status(job, out_put, MAXLIST));

    return 1;
}
int pause_job(char* id){
    JOB* job=get_job_byid(id);
    if(job==NULL)
        return 0;

    killpg(job->pgid, SIGSTOP);


    return 1;
}
int resume_job(char* id){
    JOB* job=get_job_byid(id);
    if(job==NULL)
        return 0;

    debug("pgid %d", job->pgid);
    killpg(job->pgid, SIGCONT);


    return 1;
}

void remove_completed_jobs(){
    struct timeval now;
    gettimeofday(&now, NULL);

    struct timeval result;
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){

        if(curr->next->a_job->status==COMPLETED){
            timersub(&now, &curr->next->a_job->change_time, &result);

            if(result.tv_sec>60){
                debug("sec ");
                curr->next=curr->next->next;
                continue;
            }
        }

        curr=curr->next;
    }
}


void complete_handler(int SIGNUM){
    pid_t p;
    int status;

    p=waitpid(-1, &status, WNOHANG);

    debug("signal received complete %d", p);
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){
        curr=curr->next;

        if(curr->a_job->status==RUNNING && curr->a_job->pgid==p){
            curr->a_job->status=COMPLETED;

            curr->a_job->chosen_printer->busy=0;
            gettimeofday(&curr->a_job->change_time,NULL);
            printf("%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            if(fp_out)
                fprintf(fp_out,"%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            break;
        }
    }

}

void pause_handler(int SIGNUM){
    pid_t p;
    int status;

    p=waitpid(-1, &status, WUNTRACED);

    debug("signal received pause %d", p);
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){
        curr=curr->next;

        if(curr->a_job->status==RUNNING && curr->a_job->pgid==p && WIFSTOPPED(status)){
            curr->a_job->status=PAUSED;
            gettimeofday(&curr->a_job->change_time,NULL);
            printf("%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            if(fp_out)
                fprintf(fp_out,"%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            break;
        }
    }
}

void resume_handler(int SIGNUM){
    pid_t p;
    int status;

    p=waitpid(-1, &status, WCONTINUED);

    debug("signal received resume %d", p);
    JOB_NODE* curr=&job_head;
    while(curr->next!=NULL){
        curr=curr->next;

        if(curr->a_job->status==PAUSED && curr->a_job->pgid==p && WIFCONTINUED(status)){
            curr->a_job->status=RUNNING;
            gettimeofday(&curr->a_job->change_time,NULL);
            printf("%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            if(fp_out)
                fprintf(fp_out,"%s\n", imp_format_job_status(curr->a_job, out_put, MAXLIST));
            break;
        }
    }
}