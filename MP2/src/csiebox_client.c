
#define _XOPEN_SOURCE 1         /* Required under GLIBC for nftw() */
#define _XOPEN_SOURCE_EXTENDED 1    /* Same */

#include "csiebox_client.h"
#include "csiebox_common.h"
#include "connect.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> 
#include <ftw.h>
#include <unistd.h>
#include <linux/inotify.h>
#include <sys/stat.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#define FLAG_I (IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY)

static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);
typedef struct ll{
    struct ll* next;
    char file_name[PATH_MAX];
    ino_t ino;

} list;
typedef struct {
    int wd;
    char path[PATH_MAX];
} i_storage_table;
i_storage_table* inotify_table[400];
list *hl_list ;
list *end ;
int base = 0,level = 0,num = 0;
char longest_path[PATH_MAX];
csiebox_client* c;
int i_fd;
//read config file, and connect to server
void csiebox_client_init(
  csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}

//show how to use csiebox_protocol_meta header
//other headers is similar usage
//please check out include/common.h
//using .gitignore for example only for convenience  

void list_create_next(list* l){
    l->next = (list*)malloc(sizeof(list));
    end = l->next;
}
int find_ino_num(const char* file,const struct stat *s,char *link_to){
    list* it;
    for(it = hl_list; it != end; it = it->next){
        if(it->ino == s->st_ino){
            strcpy(link_to,it->file_name);
            return 1; 
        }
    }
    end->ino = s->st_ino;
    strcpy(end->file_name,file);
    link_to = NULL;
    list_create_next(end);

    return 0;
}
void send_meta(const char*file){
    csiebox_protocol_meta req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    lstat(file, &req.message.body.stat);
    if(!S_ISDIR(req.message.body.stat.st_mode))
        md5_file(file, req.message.body.hash);
    req.message.body.pathlen = strlen(file+base);
    if (!send_message(c->conn_fd, &req, sizeof(req))) {
        fprintf(stderr, "send fail\n");
        return ;
    }
    send_message(c->conn_fd, file+base, req.message.body.pathlen);
}
int recv_header(csiebox_protocol_header *header){
    if (recv_message(c->conn_fd, header, sizeof(*header))) {
        return 1;
    }
    else{
        fprintf(stderr, "res failed\n");
        return 0;
    }
}
void send_file(const char * file){
    csiebox_protocol_file req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    lstat(file, &req.message.body.stat);
    req.message.body.pathlen = strlen(file+base);
    if (!send_message(c->conn_fd, &req, sizeof(req))) {
        fprintf(stderr, "send fail\n");
        return ;
    }
    if(req.message.body.stat.st_size > 0){
      int fd = open(file,O_RDONLY);

      char buffer[req.message.body.stat.st_size];
      memset(buffer,0,req.message.body.stat.st_size);
      read(fd,buffer,req.message.body.stat.st_size);
      send_message(c->conn_fd, file+base, req.message.body.pathlen);
      send_message(c->conn_fd,buffer,req.message.body.stat.st_size);
    }
}
void syn(const char *file, char type){
    send_meta(file);
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));    
    recv_header(&header);
    
    if(type == 'd'){
        if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
            printf("sync directory = %s\n", file);
        }
        else{
            printf("directory already exist\n");
        }
    }
    else if(type == 'f'){
        if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
            printf("file need more\n");
            send_file(file);
            memset(&header, 0, sizeof(header));    
            recv_header(&header);
            if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_FILE &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
                printf("file updated\n");
            }
            else{
                printf("file transfer error or empty file\n");
            }
        }
        else if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
            printf("file same\n");
        }

    }
    else{

    }
    return;
}
void linking(const char *file,char *link_to,char type){
    csiebox_protocol_hardlink req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    //more = hardlink || OK = symlink
    if(type == 'h'){
        req.message.header.req.status = CSIEBOX_PROTOCOL_STATUS_MORE;
        req.message.body.srclen = strlen(file+base);
        req.message.body.targetlen = strlen(link_to+base);
        if (!send_message(c->conn_fd, &req, sizeof(req))) {
            fprintf(stderr, "send fail\n");
            return ;
        }
        fprintf(stderr, "file+base=%s || link_to+base=%s\n",file+base,link_to+base );
        send_message(c->conn_fd,file+base,req.message.body.srclen);
        send_message(c->conn_fd,link_to+base,req.message.body.targetlen);
    }
    else{
        int same_head = 0;
        req.message.header.req.status = CSIEBOX_PROTOCOL_STATUS_OK;
        char sympath[PATH_MAX]="";
        if(strncmp(file,link_to,base-1) == 0){
            same_head = 1;
            strcpy(sympath,link_to+base);
        }
        else{
            strcpy(sympath,link_to);
        }
        req.message.body.srclen = strlen(file+base);
        req.message.body.targetlen = strlen(sympath);
        if (!send_message(c->conn_fd, &req, sizeof(req))) {
            fprintf(stderr, "send fail\n");
            return ;
        }
        send_message(c->conn_fd,file+base,req.message.body.srclen);
        send_message(c->conn_fd,sympath,req.message.body.targetlen);
        send_message(c->conn_fd,&same_head,sizeof(int));
    }


    fprintf(stderr, "after send link\n" );
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    recv_header(&header);
    if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
        printf("\nhard link success\n");
    }
    else if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
        printf("\nsoft link success\n");
    }
    else{
        printf("\nlink failed\n");
    }
}
void add_inotify(const char *file){
    int wd = inotify_add_watch(i_fd,file,FLAG_I);
    int i;
    for(i = 0; i < 400;i++){  
        if(inotify_table[i] != NULL) continue;
        inotify_table[i] = (i_storage_table*)malloc(sizeof(i_storage_table));
        inotify_table[i]->wd =wd;
        strcpy(inotify_table[i]->path,file);
        fprintf(stderr, "\n***INOTIFY***\nlist[%d]->wd = %d ->path = %s \n***INOTIFY***\n\n",i, inotify_table[i]->wd,inotify_table[i]->path);
        break;
    }
}
int process(const char *file, const struct stat *sb,int flag, struct FTW *s){ 
    if(s->level == 0) {
      int wd = inotify_add_watch(i_fd,file,FLAG_I);
      inotify_table[0] = (i_storage_table*)malloc( sizeof(i_storage_table));
      inotify_table[0]->wd =wd;
      strcpy(inotify_table[0]->path,file);
      fprintf(stderr, "\n***INOTIFY***\ninotify_table[0]->wd = %d ->path = %s \n***INOTIFY***\n\n", inotify_table[0]->wd,inotify_table[0]->path);
      return 0 ;
    }
    //int retval = 0;
    //char start[PATH_MAX];
    //getcwd(start,sizeof start);
    //printf("pwd = %s \n", start);
    printf("file = %s \n", file);
    const char *name = file + s->base;
   
    if (base == 0 && s->level == 1) base = s->base;
    if(s->level > level) {
        level = s->level;
        memset(longest_path,0,sizeof(longest_path));
        strcpy(longest_path,file+base);
    }

    printf("%*s", s->level * 4, "");    /* indent over */
    //printf("%d file = %s ,base = %d ,level = %d || ",num,file,s->base,s->level);
    num++;
    if(flag == FTW_F){
        printf("file:%s, hardlink num:%d \n",name,sb->st_nlink );
        if(sb->st_nlink > 1){
            char link_to[PATH_MAX];
            if(!find_ino_num(file,sb,link_to)){
                printf("save to linklist\n");
                syn(file,'f');
            }
            else{
                printf("link to: %s",link_to);
                linking(file,link_to,'h');
            }
        }
        else{
            syn(file,'f');
        }
    }
    else if(flag == FTW_D){
        fprintf(stderr,"dir:%s\n", name);
        add_inotify(file);
        syn(file,'d');
    }
    else if(flag == FTW_SL){
        fprintf(stderr, "symlink:%s\n", name);
        char buf[PATH_MAX]="";
        readlink(file,buf,PATH_MAX);
        linking(file,buf,'s');
    }
    else{
        fprintf(stderr, "what is this??\n" );
    }
    
    return 0;
}
void traverse(csiebox_client* client){
  int i,  nfds = 1;
    int flags = FTW_PHYS;
    c = client;
    nftw(client->arg.path, process, nfds, flags);
}
void create_longest_path_txt(){
    char path[PATH_MAX]="";
    sprintf(path,"%s/longestPath.txt",c->arg.path);
    int fd = open(path, O_CREAT|O_RDWR| O_TRUNC, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    write(fd,longest_path,strlen(longest_path));
    close(fd);
    syn(path,'f');
}
void change(struct inotify_event* event){
    if(strlen(event->name) == 0) return;
    char path[PATH_MAX]="";
    sprintf(path,"%s/%s",inotify_table[(event->wd)-1]->path,event->name);
    fprintf(stderr, "ino-path:%s\n", path);
    char type = 'f';
    if (event->mask & IN_ISDIR){
        type = 'd';
        add_inotify(path);
    }
    syn(path,type);
}
int find_wd(int wd){
    int i;
    for(i = 0; i< 400;i++){
        if(inotify_table[i] == NULL) continue;
        if(inotify_table[i]->wd == wd) return i;
    }
    return -1;
}
void delete(struct inotify_event* event){
    char path[PATH_MAX]="";
    sprintf(path,"%s/%s",inotify_table[find_wd(event->wd)]->path,event->name);
    fprintf(stderr, "ino-path:%s\n", path);
    int i;
    if (event->mask & IN_ISDIR){
      for(i = 0; i < 400;i++){
          if(inotify_table[i] == NULL) continue;
          if(strcmp(inotify_table[i]->path,path) == 0){
            free(inotify_table[i]);
            inotify_table[i] = NULL;
            break;
          }
      }
    }

    csiebox_protocol_rm req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    req.message.body.pathlen = strlen(path+base);
    if (!send_message(c->conn_fd, &req, sizeof(req))) {
        fprintf(stderr, "send fail\n");
        return ;
    }
    send_message(c->conn_fd, path+base, req.message.body.pathlen);

    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));    
    recv_header(&header);
    if(header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_RM &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
        printf("delete success\n");
    }
    else{
        printf("delete failed\n");
    }

}

//this is where client sends request, you sould write your code here
int csiebox_client_run(csiebox_client* client) {
  if (!login(client)) {
    fprintf(stderr, "login fail\n");
    return 0;
  }
  fprintf(stderr, "login success\n");
  

  //This is a sample function showing how to send data using defined header in common.h
  //You can remove it after you understand
  //sampleFunction(client);
  int length, i = 0;
  int wd;
  char buffer[EVENT_BUF_LEN]="";
  //====================
  i_fd = inotify_init();
  if (i_fd < 0) {
    perror("inotify_init");
  }

  hl_list = (list*)malloc(sizeof(list));
  end = hl_list;
  traverse(client);
  create_longest_path_txt();
  printf("\nlongest_path  =  %s\n", longest_path);

  //====================
  while ((length = read(i_fd, buffer, EVENT_BUF_LEN)) > 0) {
    i = 0;
    while (i < length) {
      struct inotify_event* event = (struct inotify_event*)&buffer[i];
      fprintf(stderr,"event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
      int deal = 1;
      if (event->mask & IN_CREATE) {
        deal = 1;
        fprintf(stderr,"create ");
      }
      if (event->mask & IN_ATTRIB) {
        deal = 1;
        fprintf(stderr,"attrib ");
      }
      if (event->mask & IN_MODIFY) {
        deal = 1;
        fprintf(stderr,"modify ");
      }
      
      if (event->mask & IN_DELETE) {
        deal = 0;
        fprintf(stderr,"delete ");
      }
      
      if (event->mask & IN_ISDIR) {
        fprintf(stderr,"dir\n");
      } else {
        fprintf(stderr,"file\n");
      }
      if(deal){
          change(event);
      }
      else{
          delete(event);
      }
      i += EVENT_SIZE + event->len;
    }
    memset(buffer, 0, EVENT_BUF_LEN);
  }

  return 1;
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test = test & accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      client->client_id = header.res.client_id;
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}
