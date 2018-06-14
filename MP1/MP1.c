#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include "md5sum.h"

#define STATUS 111
#define COMMIT 222
#define LOG 333
#define MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

void status();
void commit();
void get_log(int num_req);
void file_list(char*** result,int *num);
int current_status(int fd,char*new_file,char*modified,char*copied,int need_md5, char***md5, int* md5_num);
void print_status(char*new_file,char*modified,char*copied);
int collect_old_list(int fd, char*** list,char***wmd5, int* num,int commit_num);

int main(int argc, char *argv[]){  
    int mode = 0;
    chdir(argv[argc-1]);
    int config;
    if(strcmp(argv[1],"status") == 0){
        mode = STATUS;
    }
    else if(strcmp(argv[1],"commit") == 0){
        mode = COMMIT;
    }
    else if(strcmp(argv[1],"log") == 0){
        mode = LOG;
    }
    else{
        //printf("command = %s\n", argv[1]);
        config = open(".loser_config",O_RDONLY);
        if(config >= 0){
            char sub_cm[30], eq[3], true_cm[10],line[100]="";
            char cha;
            while(1){
                int left = read(config,&cha,1);

                if(cha == '\n' || left <= 0){
                    sscanf(line,"%s%s%s",sub_cm,eq,true_cm);
                    if(strcmp(sub_cm, argv[1]) == 0){
                        if(strcmp(true_cm,"status") == 0)
                            mode = STATUS;
                        if(strcmp(true_cm,"commit") == 0)
                            mode = COMMIT;
                        if(strcmp(true_cm,"log") == 0)
                            mode = LOG;
                    
                    }
                    if(mode) {
                        close(config);
                        break;
                    }
                    memset(line,0,100);
                    continue;
                }
                strncat(line,&cha,1);
                if(!left) {
                    close(config);
                    break;
                }
            }
            
        }
        else{
            fprintf(stderr,"wtf is your command\n");
        }
    }
    //start 
    if(mode == 0){
        fprintf(stderr,"go fuck yourself\n");
    }
    else if(mode == STATUS){
        status();
    }
    else if(mode == COMMIT){
        commit();
    }
    else if(mode == LOG){
        get_log(atoi(argv[2]));
    }
    return 0;
}
//this is correct
void file_list(char*** result,int *num){
    struct dirent **list;
    int n = scandir(".",&list,NULL,alphasort);
    int m = n-1;
    *num = n-2;
    (*result) = (char**)malloc((*num)*sizeof(char*));
    int count = 1;
    while(n--){
        if(strcmp(list[m-n]->d_name,".")==0 ||strcmp(list[m-n]->d_name,"..")==0)
            continue;
        if(strcmp(list[m-n]->d_name,".loser_record")==0){
            (*num) --;
            continue;
        }
        (*result)[count-1] = strdup(list[m-n]->d_name);
        free(list[m-n]);
        count++;
    }
    free(list);    
    return;
}
//this is correct
int collect_old_list(int fd, char*** list, char*** md5,int* num,int commit_num){
    int filesize = lseek(fd, -2, SEEK_END);
    int i;
    (*num) = 0;
    char line[300000] ="";
    char temp_line[1000]= "";
    filesize++; 
    while(filesize--){
        char temp;
        if(read(fd,&temp,1) >= 0){
            if(temp == '\n'){
                if(strcmp(")5DM(",temp_line) == 0){
                    break;
                }
                else{
                    int len = strlen(temp_line);
                    for(i = 0; i < len/2; i++){
                        char swap = temp_line[i];
                        temp_line[i] = temp_line[len-i-1];
                        temp_line[len-i-1] = swap;
                    }
                    sprintf(line,"%s%s\n",line,temp_line);
                    (*num)++;
                    memset(temp_line,0,1000);
                }
            }
            else{
                strncat(temp_line,&temp,1);
            }
        }
        else{
            printf("counter unexpected error\n");
            return -1;
        } 
        lseek(fd,-2,SEEK_CUR);       
    }
    (*list) = (char**)malloc((*num)*sizeof(char*));
    (*md5) = (char**)malloc((*num)*sizeof(char*));
    char* tok = strtok(line," \n");
    i = 1;
    while(tok != NULL){
        (*list)[(*num)-i] = strdup(tok);
        tok = strtok(NULL," \n");
        (*md5)[(*num)-i] = strdup(tok);
        tok = strtok(NULL," \n");
        i++;
    }

    if(commit_num == 0){ 
        return 0;
    }
    else{
        lseek(fd,-2,SEEK_CUR); 
        
        while(filesize--){
            char temp;
            if(read(fd,&temp,1) >= 0){
                if(temp == '\n' || filesize <= 0){
                    if(filesize <= 0) strncat(temp_line,&temp,1);
                    int len = strlen(temp_line);
                    for(i = 0; i < len/2; i++){
                        char swap = temp_line[i];
                        temp_line[i] = temp_line[len-i-1];
                        temp_line[len-i-1] = swap;
                    }
                    char check_commit_number[1000];
                    strncpy(check_commit_number,temp_line,8);
                    if(strcmp(check_commit_number,"# commit") == 0){
                        int commit_no;
                        sscanf(temp_line,"# commit %d",&commit_no);
                        return commit_no;
                    }
                    memset(temp_line,0,1000);
                }
                else{
                    strncat(temp_line,&temp,1);                
                }
            }
            lseek(fd,-2,SEEK_CUR);
        }
    }
    return -1;
}
int current_status(int fd,char*new_file,char*modified,char*copied,int need_md5,char***md5,int* md5_num){
    char **current_list,** prev_list, **prev_md5;
    int num_prev, num_current_list;
    int commit_number = collect_old_list(fd, &prev_list,&prev_md5, &num_prev, need_md5);
    //printf("commit_number = %d\n", commit_number);
    file_list(&current_list, &num_current_list);
    int i,j;
    if(need_md5) {
        (*md5) = (char**)malloc(num_current_list*sizeof(char*));
        (*md5_num) = num_current_list;
    }
    for(i = 0; i < num_current_list; i++){
        int filename_exist = 0;
        char *curr_md5 = sum(current_list[i]);
        if(need_md5){
            char combine[100]="";
            sprintf(combine,"%s %s\n",current_list[i],curr_md5);
            (*md5)[i] = strdup(combine);
        }
        for(j = 0; j < num_prev; j++){
            if(strcmp(current_list[i],prev_list[j]) == 0){
                filename_exist = 1;        
                if(strcmp(curr_md5,prev_md5[j]) != 0){
                    strcat((modified),current_list[i]);
                    strcat((modified),"\n");
                    //printf("modified===\n%s\n", modified);
                }
                break; 
            }
        }  
        if(!filename_exist){
            int copy = 0;
            for(j = 0; j < num_prev; j++){                
                if(strcmp(curr_md5,prev_md5[j]) == 0){
                    //asprintf((*copied),)
                    strcat((copied),prev_list[j]);
                    strcat((copied)," => ");
                    strcat((copied),current_list[i]);
                    strcat((copied),"\n");
                    copy = 1;
                    break;
                }                 
            }
            if(!copy){
                strcat((new_file),current_list[i]);
                strcat((new_file),"\n");
            }
        }
        else continue;
    }
    return commit_number;
}
void print_status(char*new_file,char*modified,char*copied){
    printf("[new_file]\n");
    if(strlen(new_file) != 0)
        printf("%s", new_file);
    printf("[modified]\n");
    if(strlen(modified) != 0)
        printf("%s", modified);
    printf("[copied]\n");
    if(strlen(copied) != 0)
        printf("%s", copied);
    return;
}
void status(){
    int record = open(".loser_record",O_RDONLY);

    if(record < 0){//record不存在，全部視為new
        char **list;
        int num;
        file_list(&list,&num);
        printf("[new_file]\n");
        int i;
        for(i = 0; i < num; i++){
            printf("%s\n", list[i]);
            free(list[i]);
        }
        free(list);
        printf("[modified]\n[copied]\n");
    }
    else{
        char new_file[300000] ="", modified[300000] ="" , copied[300000] = "";
        current_status(record,new_file,modified,copied,0,NULL,NULL);
        print_status(new_file,modified,copied);
        close(record);
    }
}
void commit(){
    int fd = open(".loser_record", O_RDWR|O_APPEND,MODE),i;
    if(fd == -1){
        char **list;
        int num;
        file_list(&list,&num);
        if(num == 0) return;
        fprintf(stderr,"create file\n");
        fd = open(".loser_record", O_RDWR|O_CREAT,MODE);
        
        write(fd,"# commit 1\n[new_file]\n",strlen("# commit 1\n[new_file]\n"));
        

        for(i = 0; i < num; i++){
            write(fd,list[i],strlen(list[i]));
            write(fd,"\n",1);
        }

        write(fd,"[modified]\n[copied]\n(MD5)\n",strlen("[modified]\n[copied]\n(md5)\n"));
        for(i = 0; i < num;i++){
            char* md5sum = sum(list[i]);
            char data[100];
            sprintf(data,"%s %s\n",list[i],md5sum);
            write(fd, data, strlen(data));
        }
        close(fd);
    }
    else{
        char ** md5;
        char new_file[300000] = "", modified[300000] = "", copied[300000] = "";
        int current_number;
        int commit_number = current_status(fd,new_file,modified,copied,1,&md5,&current_number);

        if(strlen(new_file) == 0 && strlen(modified) == 0 && strlen(copied) == 0 ){
            fprintf(stderr,"no change compare to last commit\nexit\n");
            return;
        }
        char commit_title[100];
        sprintf(commit_title,"\n# commit %d\n",commit_number+1);
        write(fd,commit_title,strlen(commit_title));
        write(fd,"[new_file]\n",strlen("[new_file]\n"));
        write(fd,new_file,strlen(new_file));
        write(fd,"[modified]\n",strlen("[modified]\n"));
        write(fd,modified,strlen(modified));
        write(fd,"[copied]\n",strlen("[copied]\n"));
        write(fd,copied,strlen(copied));
        write(fd,"(MD5)\n",strlen("pied]\n"));
        for(i = 0; i < current_number; i++){
            write(fd,md5[i],strlen(md5[i]));
        }
        fprintf(stderr,"commit %d completed\n", commit_number+1);
        close(fd);
    }

}
void get_log(int num_req){
    int fd = open(".loser_record",O_RDONLY);

    if(fd == -1) return;
    else{
        int filesize = lseek(fd,-2,SEEK_END),i;
        char line[300000] ="";
        char temp_line[1000]= "";
        filesize++;
        while(filesize-- && num_req > 0){
            
            char temp;
            if(read(fd,&temp,1) >= 0){
                if(temp == '\n' || filesize <= 0){
                    if(filesize <= 0) strncat(temp_line,&temp,1);

                    strcat(line,temp_line);
                    int len = strlen(temp_line);
                    for(i = 0; i < len/2; i++){
                        char swap = temp_line[i];
                        temp_line[i] = temp_line[len-i-1];
                        temp_line[len-i-1] = swap;
                    }
                    char commit_message[10];
                    strncpy(commit_message,temp_line,8);
                    if(strcmp(commit_message,"# commit") == 0){                        
                        int l = strlen(line);
                        for(i = 0; i < l; i++){
                            printf("%c", line[l-i-1]);
                        }
                        puts("");
                        memset(line,0,300000);
                        num_req--;
                        if(num_req && filesize) puts("");
                    }
                    if(line[0] != 0) strcat(line,"\n");
                    memset(temp_line,0,1000);
                }
                else{
                    strncat(temp_line,&temp,1);                
                }
                lseek(fd,-2,SEEK_CUR);
            }
        }
    }
    return;
}
