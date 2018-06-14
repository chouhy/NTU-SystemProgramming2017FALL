#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_sum(char *CHARSET, int *count){
    int len = strlen(CHARSET);
    int sum = 0;
    if(len == 0) printf("0\n");
    else{
        for(int i = 0; i < len; i++){
            sum += count[CHARSET[i]];
        }
        printf("%d\n", sum);
    }
    
    for(int i = 0; i < 256;i++)
        count[i] = 0;
    return;
}
int main(int argc, char *argv[])
{
    /*
    input issue
    */
    FILE *fp;
    char* CHARSET;
    if(argc < 2){
        fprintf(stderr, "NEED MORE ARGUMENTS\n" );
        return 0;
    }
    else if(argc == 2){
        CHARSET = argv[1];
        fp = stdin;
    }
    else if(argc == 3){
        CHARSET = argv[1];
        fp = fopen(argv[2],"rb");
        if(fp == NULL){ 
            fprintf(stderr, "error\n" );
            return 0;
        }
    }
    else{ 
        fprintf(stderr, "TOO MUCH ARGUMENTS\n" );
        return 0;
    }
    /*
    loop->result
    */
    int count[256] = {0};
    while(1){
        char cha = fgetc(fp);
        if(cha == EOF){
            break;
        }
        else if(cha == '\n'){
            print_sum(CHARSET, count);
            continue;
        }
        else
        count[cha]++;
    }
    return 0;
}