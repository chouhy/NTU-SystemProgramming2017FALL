#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <dlfcn.h>
#include <gnu/lib-names.h> 
#include "cJSON.h"
#define BUFF_SIZE 4096
#define MAX_FD 1024

struct User {
    char name[33];
    unsigned int age;
    char gender[7];
    char introduction[1025];
};

void build_user(char* data,struct User *user){
    if(data == NULL) return;
    cJSON *root = cJSON_Parse(data);
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root,"name");
    cJSON *age = cJSON_GetObjectItemCaseSensitive(root,"age");
    cJSON *gender = cJSON_GetObjectItemCaseSensitive(root,"gender");
    cJSON *intro = cJSON_GetObjectItemCaseSensitive(root,"introduction");
    //cJSON *filter = cJSON_GetObjectItemCaseSensitive(root,"filter_function");
    strcpy(user->name,name->valuestring);
    user->age = age->valueint;
    strcpy(user->gender,gender->valuestring);
    strcpy(user->introduction,intro->valuestring);
    cJSON_Delete(root);
}
char *process_json_format(char* str){
    int i = 0,j,len = strlen(str);
    char *ret = malloc(len);
    memset(ret,0,len);
    for(j = 0; j < len;j++){
        if(str[j] != '\n'){
            ret[i] = str[j];
            i++;
        }
    }
    sprintf(ret,"%s\n",ret);
    return ret;
}
int put_in_user_data(int fd,char ** user_data, char * data){
    user_data[fd] = strdup(data);
    cJSON *root = cJSON_Parse(data);
    cJSON *filter_function = cJSON_GetObjectItemCaseSensitive(root,"filter_function");
    char *data_type = "struct User {char name[33]; unsigned int age;char gender[7];char introduction[1025];};\n";
    char name[10]="";
    sprintf(name,"%d.c",fd);
    FILE *fp = fopen(name,"w");
    fprintf(fp, "%s%s\n", data_type,filter_function->valuestring);
    fclose(fp);
    char cmd[100]="";
    sprintf(cmd,"gcc -fPIC -O2 -std=c11 %s -shared -o %d.so",name,fd);
    system(cmd);
    cJSON_Delete(root);
    return 0;
}
int main(int argc, char const *argv[]) {

    int available[1000] ={0};//filedescripter as index
    int chatting_to[1000];//filedescripter as index
    memset(chatting_to,-1,1000*sizeof(int));
    char **user_data = (char**)malloc(1000*sizeof(char*)); 
    memset(user_data,0,1000*sizeof(char*));   
    int i,j,port = atoi(argv[1]);// 宣告 socket 檔案描述子
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);// 利用 struct sockaddr_in 設定伺服器"自己"的位置
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr)); // 清零初始化，不可省略
    server_addr.sin_family = PF_INET;              // 位置類型是網際網路位置
    server_addr.sin_addr.s_addr = INADDR_ANY;      // INADDR_ANY 是特殊的IP位置，表示接受所有外來的連線
    server_addr.sin_port = htons(port);           // 在 44444 號 TCP 埠口監聽新連線

    // 綁定位置
    //bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)); // 綁定 sockfd 的位置
    // 使用 bind() 將伺服socket"綁定"到特定 IP 位置
    int retval = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if(retval) {
        printf("socket fail\n");
    }
    assert(!retval);

    // 呼叫 listen() 告知作業系統這是一個伺服socket
    retval = listen(sockfd, 5);
    assert(!retval);

    // 宣告 select() 使用的資料結構
    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);

    // 將 socket 檔案描述子放進 readset
    FD_SET(sockfd, &readset);
    fprintf(stderr,"sockfd = %d\n",sockfd);
    while (1) {
        memcpy(&working_readset, &readset, sizeof(fd_set));
        retval = select(MAX_FD, &working_readset, NULL, NULL, NULL);

        if (retval < 0) { // 發生錯誤
            perror("select() went wrong");
            exit(errno);
        }

        if (retval == 0) { // 排除沒有事件的情形
            continue;
        }
        int fd;
        for (fd = 0; fd < MAX_FD; fd ++) { // 用迴圈列舉描述子
            // 排除沒有事件的描述子
            if (!FD_ISSET(fd, &working_readset))
                continue;
            fprintf(stderr,"fd = %d\n",fd);
// 分成兩個情形：接受新連線用的 socket 和資料傳輸用的 socket
            if (fd == sockfd){
// sockfd 有事件，表示有新連線
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept(fd, (struct sockaddr*) &client_addr, &addrlen);
                if (client_fd >= 0) {
                    
                    FD_SET(client_fd, &readset); // 加入新創的描述子，用於和客戶端連線
                    fprintf(stderr,"new connection\n");
                }
            }
            else {
// 這裏的描述子來自 accept() 回傳值，用於和客戶端連線
                ssize_t sz;
                char buffer[BUFF_SIZE];
                memset(buffer,0,BUFF_SIZE);
                sz = recv(fd, buffer,BUFF_SIZE , 0); // 接收資料

                if (sz == 0) { // recv() 回傳值爲零表示客戶端已關閉連線
                    if(user_data[fd] != NULL){
                        free(user_data[fd]);
                        user_data[fd] = NULL;
                        available[fd] = 0;
                    }
                    if(chatting_to[fd] >= 0){
                        char *mes = "{\"cmd\": \"other_side_quit\"}\n";
                        send(chatting_to[fd],mes,strlen(mes),0);
                        free(user_data[chatting_to[fd]]);
                        user_data[chatting_to[fd]] = NULL;
                        chatting_to[chatting_to[fd]] = -1;
                        available[chatting_to[fd]] = 0;
                        chatting_to[fd] = -1;
                    }
                    // 關閉描述子並從 readset 移除
                    fprintf(stderr,"end of connection\n");
                    close(fd);
                    FD_CLR(fd, &readset);
                }
                else if (sz < 0) { // 發生錯誤

                    /* 進行錯誤處理
                       ...略...  */
                    fprintf(stderr,"error %d\n",errno);
                    exit(errno);
                }
                else { // sz > 0，表示有新資料讀入

                    /* 進行資料處理
                       ...略...   */
                    fprintf(stderr,"%s",buffer);
                    //cJSON...
                    cJSON *root = cJSON_Parse(buffer);
                    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root,"cmd");
                    cJSON *respond_json = cJSON_CreateObject();
                    if(strcmp(cmd->valuestring,"try_match") == 0){
                        
                        cJSON_AddStringToObject(respond_json,"cmd","try_match");
                        char *str = cJSON_Print(respond_json);
                        char *res= process_json_format(str);//need to fix '\n' issue
                        free(str);
                        cJSON_Delete(respond_json);
                        cJSON_Delete(root);
                        send(fd,res, strlen(res),0);
                        free(res);

                        int user_id = fd;
                        put_in_user_data(fd,user_data, buffer);
                        fprintf(stderr, "user_id %d\n%s\n", user_id,user_data[fd]);
                        struct User self;
                        build_user(user_data[user_id],&self);

                        void *handle;
                        char dlname[20] = "";
                        sprintf(dlname,"./%d.so",user_id);
                        int (*filter1)(struct User);
                        handle = dlopen(dlname, RTLD_LAZY);
                        if(!handle) fprintf(stderr, "noooooo\n" );
                        filter1 = (int(*)(struct User))dlsym(handle,"filter_function");
                        available[user_id] = 1;
                        int match_id = -1;
                        for(i = 0; i < 1000; i++){
                            //fprintf(stderr, "i %d\n", i);
                            if(i == user_id) continue;
                            if(!available[i]) continue;
                            struct User temp;
                            if(user_data[i] != NULL){
                                build_user(user_data[i],&temp);
                                fprintf(stderr,"name= %s\n",temp.name);
                                fprintf(stderr,"age = %d\n",temp.age);
                                void *handle2;
                                char dlname2[20]="";
                                
                                sprintf(dlname2,"./%d.so",i);
                                int (*filter2)(struct User);                                
                                handle2 = dlopen(dlname2,RTLD_LAZY);
                                dlerror();                                
                                filter2 = (int(*)(struct User))dlsym(handle2,"filter_function");
                                if(filter1(temp) && filter2(self)){
                                    match_id = i;
                                    fprintf(stderr, "matched!!!!\n" );
                                    dlclose(handle2);
                                    break;
                                }
                            } 
                        } 
                        dlclose(handle);
                        
                        if(match_id != -1){
                            available[user_id] = 0;
                            available[match_id] = 0;
                            chatting_to[user_id] = match_id;
                            chatting_to[match_id] = user_id;
                            cJSON *match1 = cJSON_Parse(user_data[user_id]);
                            cJSON_ReplaceItemInObjectCaseSensitive(match1,"cmd",cJSON_CreateString("matched"));
                            cJSON *matched = cJSON_Parse(user_data[match_id]);
                            cJSON_ReplaceItemInObjectCaseSensitive(matched,"cmd",cJSON_CreateString("matched"));
                            char *match_raw1 = cJSON_Print(match1);
                            char *match_raw2 = cJSON_Print(matched);
                            char *match_res1 = process_json_format(match_raw1);
                            char *match_res2 = process_json_format(match_raw2);

                            send(match_id,match_res1,strlen(match_res1),0);
                            send(user_id,match_res2,strlen(match_res2),0);

                            cJSON_Delete(match1);
                            cJSON_Delete(matched);
                            free(match_raw1);
                            free(match_raw2);
                            free(match_res1);
                            free(match_res2);
                        }
                        
                    }
                    else if(strcmp(cmd->valuestring,"send_message") == 0){
                        send(fd,buffer,strlen(buffer),0);
                        cJSON_ReplaceItemInObjectCaseSensitive(root,"cmd",cJSON_CreateString("receive_message"));
                        char *tmp = cJSON_Print(root);
                        char *send_msg = process_json_format(tmp);
                        send(chatting_to[fd],send_msg,strlen(send_msg),0);
                        free(tmp);
                        free(send_msg);
                        cJSON_Delete(root);
                    }
                    else if(strcmp(cmd->valuestring,"quit") == 0){
                        send(fd,buffer,strlen(buffer),0);
                        free(user_data[fd]);
                        user_data[fd] = NULL;
                        available[fd] = 0;
                        if(chatting_to[fd] >= 0){                            
                            char *mes = "{\"cmd\": \"other_side_quit\"}\n";
                            send(chatting_to[fd],mes,strlen(mes),0);
                            free(user_data[chatting_to[fd]]);
                            user_data[chatting_to[fd]] = NULL;
                            chatting_to[chatting_to[fd]] = -1;
                            available[chatting_to[fd]] = 0;
                            chatting_to[fd] = -1;
                        }
                    }

                }
            }
        }
    }

// 結束程式前記得關閉 sockfd
    close(sockfd);

    return 0;
}
