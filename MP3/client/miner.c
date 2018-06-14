#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <linux/limits.h>
#include <sys/select.h>
#include <string.h>

struct test_set{
    unsigned char set[PATH_MAX];
    int digit;
};
void init(struct test_set* a, int lower){
    memset(a->set,0,PATH_MAX);
    a->set[0] = lower;
    a->digit = 1;
}
unsigned char* mMD5(unsigned char * str){
    unsigned char digest[16];    
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, str, strlen(str));
    MD5_Final(digest, &context);
    unsigned char* final = malloc(33);
    for(int i = 0; i < 16;i++){
        sprintf(&final[i*2],"%.2x",(unsigned int)digest[i]);
    }
    return final;
}
int verify(unsigned char* test,int num){
    if(test[num] == '0') return 0;
    for(int i = 0; i < num;i++){
        if(test[i] != '0') return 0;
    }
    return 1;
}
int add(unsigned char* s,int d,int low,int high){
    //printf("d = %d\n", d);
    if(d == 0){
        if(s[d] != high-1){
            //printf("s[d] %d\n", s[d]);
            s[d]++;
            return 0;
        }
        else{
            s[d] = low;
            //printf("here\n");
            return 1;
        }
    }
    if(s[d] != 255){
        s[d]++;
        return 0;  
    } 
    s[d] = 0;
    return add(s,d-1,low,high);
}
void testset_next(struct test_set* s,int lower,int higher){
    //printf("low=%d  high=%d\n", first_low,first_high);
    s->digit += add(s->set,s->digit-1,lower,higher);
    //printf("digit = %d\n", s->digit-1);
}
int main(int argc, char **argv)
{
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    assert (!ret);

    /* open pipes */
    

    /* TODO write your own (1) communication protocol (2) computation algorithm */
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */
    while(1){
        int input_fd = open(input_pipe, O_RDONLY);
        assert (input_fd >= 0);

        int output_fd = open(output_pipe, O_WRONLY);
        assert (output_fd >= 0);
        int ini = 1;
        read(input_fd,&ini,sizeof(int));
        if(ini == 0){
            printf("BOSS is mindful.\n");
            ini = 1;
        }

        fd_set readset;
        fd_set working_readset;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);

        int n_treasure, length;
        unsigned char mine[PATH_MAX]="";
        int all_split,num_split,work = 0;
        struct test_set test;
        
        unsigned char *hash = NULL;
        while(1){
            memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
            select(input_fd+1, &working_readset, NULL, NULL, &timeout);

            if(FD_ISSET(input_fd,&working_readset)){
                int cmd = 0;
                read(input_fd,&cmd,sizeof(int));
                if(cmd == 1){
                    
                    work = 1;
                    memset(mine,0,PATH_MAX);
                    read(input_fd,&n_treasure,sizeof(int));
                    read(input_fd,&length,sizeof(int));
                    read(input_fd,mine,length);
                    read(input_fd,&all_split,sizeof(int));
                    read(input_fd,&num_split,sizeof(int));
                    fprintf(stderr,"mine = %s\n", mine);
                    
                    init(&test,(256*num_split)/all_split);
                }
                else if(cmd == 2){
                    work = 0;
                    int damn_guy_len, find_n;
                    char damn_guy[20],n_treasure_hash[33];
                    memset(damn_guy,0,20);
                    memset(n_treasure_hash,0,33);
                    read(input_fd,&damn_guy_len,sizeof(int));
                    read(input_fd,damn_guy,damn_guy_len);
                    read(input_fd,&find_n,sizeof(int));
                    read(input_fd,n_treasure_hash,33);

                    if(strcmp(damn_guy,name) != 0){
                        printf("%s wins a %d-treasure! %s\n",damn_guy,find_n,n_treasure_hash );
                    }
                    else{
                        printf("I win a %d-treasure! %s\n", find_n,n_treasure_hash);
                    }

                }
                else if(cmd == 3) {
                    work = 0;
                    printf("BOSS is at rest.\n");
                    close(input_fd);
                    close(output_fd);
                    break;
                }
                else if(cmd == 4){
                    printf("I'm working on %s\n", hash);
                }
            }
            if(work){
                int lower = (256*num_split)/all_split,higher = (256*(num_split+1))/all_split;
                //fprintf(stderr, "aaa = %d\n", aaa++);
                unsigned char temp[PATH_MAX] = "";
                strcpy(temp,mine);
                strncat(temp,test.set,test.digit);
                //fprintf(stderr, "1\n" );
                if(hash != NULL){
                    free(hash);
                    hash = NULL;
                }
                //fprintf(stderr, "2\n" );
                hash = mMD5(temp);
                //fprintf(stderr, "3\n" );
                //printf("hash: %s\n", hash);
                int valid = verify(hash,n_treasure);
                //fprintf(stderr, "4\n" );
                //if(test.set[0] < 33) valid = 0;
                if(valid){
                    //TODO
                    work = 0;
                    //printf("I win a %d-treasure! %s\n", n_treasure,hash);
                    /*
                    char degggg[PATH_MAX]="add = ";
                    strncat(degggg,test.set,test.digit);
                    printf("%s %d\n", degggg,test.digit);
                    exit(0);*/
                    int found = 1,name_len = strlen(name),temp_len = strlen(temp);
                    write(output_fd,&found,sizeof(int));
                    write(output_fd,hash,33);
                    write(output_fd,&name_len,sizeof(int));
                    write(output_fd,name,name_len);
                    write(output_fd,&temp_len,sizeof(int));
                    write(output_fd,temp,temp_len);
                    write(output_fd,&n_treasure,sizeof(int));
                } 
                else{
                    testset_next(&test,lower,higher);
                    
                }
                //fprintf(stderr, "5\n" );
            }
        }
    }
    return 0;
}
