#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/select.h>

#include "boss.h"

int conn_num;
int load_config_file(struct server_config *config, char *path)
{
    int number_miners = 0;
    struct pipe_pair *pipes = (struct pipe_pair *)malloc(8*sizeof(struct pipe_pair));
    FILE *fp = fopen(path,"r");
    char tmp[PATH_MAX] ="";
    while(fscanf(fp,"%s",tmp) != EOF){
        if(strcmp(tmp,"MINE:") == 0){
            fscanf(fp,"%s",tmp);
            config->mine_file  = strdup(tmp)/* path to mine file */;
            
        }
        else if(strcmp(tmp,"MINER:") == 0){
            char tmp2[PATH_MAX]="" ;
            fscanf(fp,"%s %s",tmp,tmp2);
            pipes[number_miners].input_pipe = strdup(tmp);
            pipes[number_miners].output_pipe = strdup(tmp2);            
            number_miners ++;
        }
    }
    /* TODO finish your own config file parser */
    
    config->pipes      = pipes/* array of pipe pairs */;
    config->num_miners = number_miners /* number of miners */;
    fclose(fp);
    return 0;
}

int assign_jobs(unsigned char* mine,struct fd_pair* fds,int target)
{
    /* TODO design your own (1) communication protocol (2) job assignment algorithm */

    int job = 1,i;
    for(i = 0; i < conn_num; i++){
        write(fds[i].input_fd, &job,sizeof(int));
        write(fds[i].input_fd, &target,sizeof(int));
        int length = strlen(mine);
        write(fds[i].input_fd, &length,sizeof(int));
        write(fds[i].input_fd, mine,length);
        write(fds[i].input_fd, &conn_num,sizeof(int));
        write(fds[i].input_fd, &i,sizeof(int));
    }
}


void debug(struct server_config *config){
    fprintf(stderr,"%s\n", config->mine_file);
    int i;
    for(i = 0; i < config->num_miners;i++){
        fprintf(stderr,"in %s out %s\n", config->pipes[i].input_pipe,config->pipes[i].output_pipe);
    }
}
unsigned char* mMD5(unsigned char * str){
    unsigned char digest[16];    
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, str, strlen(str));
    MD5_Final(digest, &context);
    unsigned char* final = malloc(33);
    int i;
    for(i = 0; i < 16;i++){
        sprintf(&final[i*2],"%.2x",(unsigned int)digest[i]);
    }
    return final;
}
int current_treasure(unsigned char* s){
    unsigned char* temp = mMD5(s);
    int i;
    for( i = 0; i < 32;i++){
        if(temp[i] != '0') {
            free(temp);
            return i+1;
        }
    }
    return 32;
}
void read_mine(int fd,unsigned char * tmp){
    char a;
    while(read(fd,&a,1)){
        strncat(tmp,&a,1);
    }
}
int main(int argc, char **argv)
{
    /* sanity check on arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    /* load config file */
    struct server_config config;
    load_config_file(&config, argv[1]);
    debug(&config);
    /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];
    conn_num = config.num_miners;
    int ind;
    for ( ind = 0; ind < config.num_miners; ind += 1)
    {
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];

        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_WRONLY);
        assert (fd_ptr->input_fd >= 0);

        int init = 0;
        write(fd_ptr->input_fd,&init,sizeof(int));

        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_RDONLY);
        assert (fd_ptr->output_fd >= 0);
    }

    /* initialize data for select() */
    int maxfd = 0;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    // TODO add input pipes to readset, setup maxfd
    int i;
    for(i = 0; i < config.num_miners; i++){
        FD_SET(client_fds[i].output_fd, &readset);
        if(client_fds[i].output_fd > maxfd) maxfd = client_fds[i].output_fd;
    }
    maxfd++;
    /* assign jobs to clients */
    unsigned char *mine,tmp[PATH_MAX];
    memset(tmp,0,PATH_MAX);

    //FILE *fp = fopen(config.mine_file,"r");
    //fscanf(fp,"%s",tmp);

    int fdd = open(config.mine_file,O_RDONLY);
    read_mine(fdd,tmp);
    mine = strdup(tmp);

    int current_target = current_treasure(mine);
    //printf("current_target = %d,\n", current_target);
    assign_jobs(mine,client_fds,current_target);

    while (1)
    {
        memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        select(maxfd, &working_readset, NULL, NULL, &timeout);

        if (FD_ISSET(STDIN_FILENO, &working_readset))
        {
            /* TODO handle user input here */
            char cmd[20];
            scanf("%s",cmd);
            if (strcmp(cmd, "status")==0){
                /* TODO show status */
                if(current_target == 1){
                    printf("best 0-treasure in 0 bytes\n");
                }
                else{
                    char *temp_md5 = mMD5(mine);
                    printf("best %d-treasure %s in %d bytes\n", current_target-1, temp_md5,strlen(mine));
                    free(temp_md5);
                }
                int j;
                for(j = 0; j < conn_num; j++){
                    int status = 4;
                    write(client_fds[j].input_fd,&status,sizeof(int));
                }
            }
            else if (strcmp(cmd, "dump")==0){
                /* TODO write best n-treasure to specified file */
                char dest[PATH_MAX] ="";
                scanf("%s",dest);
                int dump_fd = open(dest,O_CREAT|O_WRONLY|O_NONBLOCK|O_TRUNC,0644);
                write(dump_fd,mine,strlen(mine));
                close(dump_fd);
                
            }
            else if(strcmp(cmd, "quit")==0){

                int j;
                for(j = 0; j < conn_num; j++){
                    int quit = 3;
                    write(client_fds[j].input_fd,&quit,sizeof(int));
                }
                exit(0);
        /* TODO tell clients to cease their jobs and exit normally */
            }
            //handle_command(&client_fds);
        }

        /* TODO check if any client send me some message
           you may re-assign new jobs to clients*/
        else{
            int name_len,str_len,treasure_n;
            unsigned char treasure_hash[33], new_mine[PATH_MAX],miner_name[20];
            for(i = 0; i < conn_num;i++){
                if(FD_ISSET(client_fds[i].output_fd, &working_readset)){
                    int rpy;
                    read(client_fds[i].output_fd,&rpy,sizeof(int));
                    if(rpy == 1){
                        
                        memset(treasure_hash,0,33);
                        memset(new_mine,0,PATH_MAX);
                        memset(miner_name,0,20);
                        read(client_fds[i].output_fd,treasure_hash,33);
                        read(client_fds[i].output_fd,&name_len,sizeof(int));
                        read(client_fds[i].output_fd,miner_name,name_len);
                        read(client_fds[i].output_fd,&str_len,sizeof(int));
                        read(client_fds[i].output_fd,new_mine,str_len);  
                        read(client_fds[i].output_fd,&treasure_n,sizeof(int));
                        if(treasure_n == current_target){
                            printf("A %d-treasure discovered! %s\n", treasure_n, treasure_hash);
                            current_target++;
                            int j;
                            for(j = 0; j < conn_num; j++){
                                int broadcast = 2;
                                write(client_fds[j].input_fd,&broadcast,sizeof(int));
                                write(client_fds[j].input_fd,&name_len,sizeof(int));
                                write(client_fds[j].input_fd,miner_name,name_len);
                                write(client_fds[j].input_fd,&treasure_n,sizeof(int));
                                write(client_fds[j].input_fd,treasure_hash,33);

                            }
                            free(mine);
                            mine = strdup(new_mine);
                            //if(current_target < 7)
                            assign_jobs(mine,client_fds,current_target);
                        }
                        //broadcast and assign new job

                        

                        

                    }
                }
            }
        }
       
    }

    /* TODO close file descriptors */

    return 0;
}
