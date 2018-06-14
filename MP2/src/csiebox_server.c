#include "csiebox_server.h"

#include "csiebox_common.h"
#include "connect.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include <stdlib.h>
#include <utime.h>

csiebox_server* s;
csiebox_client_info* i;
static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(csiebox_server* server, int conn_fd);
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory

//read config file, and start to listen
void csiebox_server_init(
  csiebox_server** server, int argc, char** argv) {
  csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
  if (!tmp) {
    fprintf(stderr, "server malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_server));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = server_start();
  if (fd < 0) {
    fprintf(stderr, "server fail\n");
    free(tmp);
    return;
  }
  tmp->client = (csiebox_client_info**)
      malloc(sizeof(csiebox_client_info*) * getdtablesize());
  if (!tmp->client) {
    fprintf(stderr, "client list malloc fail\n");
    close(fd);
    free(tmp);
    return;
  }
  memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
  tmp->listen_fd = fd;
  *server = tmp;
}

//wait client to connect and handle requests from connected socket fd
int csiebox_server_run(csiebox_server* server) {
  int conn_fd, conn_len;
  struct sockaddr_in addr;
  while (1) {
    memset(&addr, 0, sizeof(addr));
    conn_len = 0;
    // waiting client connect
    conn_fd = accept(
      server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
    if (conn_fd < 0) {
      if (errno == ENFILE) {
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          fprintf(stderr, "accept err\n");
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
    }
    // handle request from connected socket fd
    handle_request(server, conn_fd);
  }
  return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
  csiebox_server* tmp = *server;
  *server = 0;
  if (!tmp) {
    return;
  }
  close(tmp->listen_fd);
  int i = getdtablesize() - 1;
  for (; i >= 0; --i) {
    if (tmp->client[i]) {
      free(tmp->client[i]);
    }
  }
  free(tmp->client);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
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
  int accept_config_total = 2;
  int accept_config[2] = {0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(server->arg.path)) {
        strncpy(server->arg.path, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("account_path", key) == 0) {
      if (vallen <= sizeof(server->arg.account_path)) {
        strncpy(server->arg.account_path, val, vallen);
        accept_config[1] = 1;
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

//It is a sample function
//you may remove it after understanding

void res_header(int conn_fd, csiebox_protocol_header* header){
  if(!send_message(conn_fd, header, sizeof(*header))){
      fprintf(stderr, "send fail\n");
      return;
  }
}
void syn_file(int conn_fd,csiebox_protocol_file *file){
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
        
    if(file->message.body.stat.st_size <= 0) {
      printf("empty file...\n");
      res_header(conn_fd,&header);
      return;
    }
    char buff[PATH_MAX]="";
    char *homedir = get_user_homedir(s,i);
    char path[PATH_MAX]="";
    strcpy(path,homedir);
    free(homedir);
        header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
        header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
        header.res.datalen = 0;
    recv_message(conn_fd, buff, file->message.body.pathlen);
        
    sprintf(path,"%s/%s",path,buff);
    char file_buf[file->message.body.stat.st_size];
    memset(file_buf,0,file->message.body.stat.st_size);
    recv_message(conn_fd,file_buf,file->message.body.stat.st_size);

    int fd = open(path, O_WRONLY | O_TRUNC);
    if(write(fd,file_buf,file->message.body.stat.st_size) <= 0){
        printf("write into file error\n");
        header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    }
    else{
        header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    }
    res_header(conn_fd,&header);
    close(fd);  

        chmod(path,file->message.body.stat.st_mode);
        struct utimbuf new_time;
        new_time.actime = file->message.body.stat.st_atime;
        new_time.modtime = file->message.body.stat.st_mtime;
        utime(path,&new_time);
}
void syn_meta(int conn_fd, csiebox_protocol_meta* meta){
    char buff[PATH_MAX];
    memset(buff,0,PATH_MAX);
    char *homedir = get_user_homedir(s,i);
    char path[PATH_MAX];
    strcpy(path,homedir);
    free(homedir);
    recv_message(conn_fd, buff, meta->message.body.pathlen);
    csiebox_protocol_header header;
        memset(&header, 0, sizeof(header));
        header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
        header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
        header.res.datalen = 0;
    sprintf(path,"%s/%s",path,buff);

    struct stat temp;
    uint8_t hash[MD5_DIGEST_LENGTH];
    memset(&hash, 0, sizeof(hash));
    int available = lstat(path,&temp);


    if(errno == ENOENT && S_ISDIR(meta->message.body.stat.st_mode)){
        mkdir(path,DIR_S_FLAG);
        printf("dir path = %s\n", path);
        //        
        header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
        chmod(path,meta->message.body.stat.st_mode);
        struct utimbuf new_time;
        new_time.actime = meta->message.body.stat.st_atime;
        new_time.modtime = meta->message.body.stat.st_mtime;
        utime(path,&new_time);

        res_header(conn_fd,&header);

    }
    else if(errno == ENOENT && S_ISREG(meta->message.body.stat.st_mode)){
        int fd = open(path, O_CREAT|O_RDWR, REG_S_FLAG);
        header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
        res_header(conn_fd,&header);
        printf("file :%s created\n", path);
        close(fd);
    }
    else{
        if(!S_ISDIR(temp.st_mode)){
            md5_file(path, hash);
            if (memcmp(hash, meta->message.body.hash, sizeof(hash)) == 0) {
                printf("hashes are equal!\n");
                header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;

                chmod(path,meta->message.body.stat.st_mode);
                struct utimbuf new_time;
                new_time.actime = meta->message.body.stat.st_atime;
                new_time.modtime = meta->message.body.stat.st_mtime;
                utime(path,&new_time);
            }
            else{
                header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
            }
            

            res_header(conn_fd,&header);
        }
        else {
            header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;

            chmod(path,meta->message.body.stat.st_mode);
            struct utimbuf new_time;
            new_time.actime = meta->message.body.stat.st_atime;
            new_time.modtime = meta->message.body.stat.st_mtime;
            utime(path,&new_time);

            res_header(conn_fd,&header);
        }
    }
}
void syn_hardlink(int  conn_fd, csiebox_protocol_hardlink* hardlink){
    char buff[PATH_MAX];
    memset(buff,0,PATH_MAX);
    char *homedir = get_user_homedir(s,i);
    char path[PATH_MAX];
    strcpy(path,homedir);
    free(homedir);
    char src[PATH_MAX],tar[PATH_MAX];
    memset(src,0,PATH_MAX);
    memset(tar,0,PATH_MAX);
    csiebox_protocol_header header;
    memset(&header,0,sizeof(header));
    header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    header.res.datalen = 0;

    recv_message(conn_fd,src,hardlink->message.body.srclen);
    recv_message(conn_fd,tar,hardlink->message.body.targetlen);
    char source[PATH_MAX]="",target[PATH_MAX]="";
    if(hardlink->message.header.req.status == CSIEBOX_PROTOCOL_STATUS_MORE){
        fprintf(stderr, "src:%s  \ntar:%s\n",src,tar );
        
        sprintf(source,"%s/%s",path,src);
        sprintf(target,"%s/%s",path,tar);
        fprintf(stderr, "src:%s  \ntar:%s\n",source,target );
        if(link(target,source) == 0){
            header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
            fprintf(stderr,"send hardlink result\n");
            
        }
        else{
            header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
        }        
    }
    //hardlink = MORE symlink = OK
    else{
        int same_head;
        recv_message(conn_fd,&same_head,sizeof(same_head));
        sprintf(source,"%s/%s",path,src);
        if(same_head){
            sprintf(target,"%s/%s",path,tar);
        }
        else{
            strcpy(target,tar);
        }
        symlink(target,source);
        header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
    }
    res_header(conn_fd,&header);
}
void delete(int  conn_fd, csiebox_protocol_rm* rm){
    char buff[PATH_MAX]="";
    char *homedir = get_user_homedir(s,i);
    char path[PATH_MAX]="";
    strcpy(path,homedir);
    free(homedir);
    
    recv_message(conn_fd,buff,rm->message.body.pathlen);
    sprintf(path,"%s/%s",path,buff);
    char command[PATH_MAX]="";
    sprintf(command,"rm -rf %s",path);
    system(command);

    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    header.res.op = CSIEBOX_PROTOCOL_OP_RM;
    header.res.datalen = 0;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    res_header(conn_fd,&header);

}
//this is where the server handle requests, you should write your code here
static void handle_request(csiebox_server* server, int conn_fd) {
  csiebox_protocol_header header;
  s = server;
  memset(&header, 0, sizeof(header));
  while (recv_message(conn_fd, &header, sizeof(header))) {
    if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
      continue;
    }
    switch (header.req.op) {
      case CSIEBOX_PROTOCOL_OP_LOGIN:
        fprintf(stderr, "login\n");
        csiebox_protocol_login req;
        if (complete_message_with_header(conn_fd, &header, &req)) {
          login(server, conn_fd, &req);
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_META:
        fprintf(stderr, "sync meta\n");
        csiebox_protocol_meta meta;
        if (complete_message_with_header(conn_fd, &header, &meta)) {
          
        //This is a sample function showing how to send data using defined header in common.h
        //You can remove it after you understand
        //sampleFunction(conn_fd, &meta);
        syn_meta(conn_fd, &meta);
          //====================
          //        TODO
          //====================
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_FILE:
        fprintf(stderr, "sync file\n");
        csiebox_protocol_file file;
        if (complete_message_with_header(conn_fd, &header, &file)) {
          syn_file(conn_fd,&file);
          //====================
          //        TODO
          //====================
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK:
        fprintf(stderr, "sync hardlink\n");
        csiebox_protocol_hardlink hardlink;
        if (complete_message_with_header(conn_fd, &header, &hardlink)) {
          syn_hardlink(conn_fd,&hardlink);
          //====================
          //        TODO
          //====================
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_END:
        fprintf(stderr, "sync end\n");
        csiebox_protocol_header end;
          //====================
          //        TODO
          //====================
        break;
      case CSIEBOX_PROTOCOL_OP_RM:
        fprintf(stderr, "rm\n");
        csiebox_protocol_rm rm;
        if (complete_message_with_header(conn_fd, &header, &rm)) {
          delete(conn_fd,&rm);
          //====================
          //        TODO
          //====================
        }
        break;
      default:
        fprintf(stderr, "unknown op %x\n", header.req.op);
        break;
    }
  }
  fprintf(stderr, "end of connection\n");
  logout(server, conn_fd);
}

//open account file to get account information
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info) {
  FILE* file = fopen(server->arg.account_path, "r");
  if (!file) {
    return 0;
  }
  size_t buflen = 100;
  char* buf = (char*)malloc(sizeof(char) * buflen);
  memset(buf, 0, buflen);
  ssize_t len;
  int ret = 0;
  int line = 0;
  while ((len = getline(&buf, &buflen, file) - 1) > 0) {
    ++line;
    buf[len] = '\0';
    char* u = strtok(buf, ",");
    if (!u) {
      fprintf(stderr, "illegal form in account file, line %d\n", line);
      continue;
    }
    if (strcmp(user, u) == 0) {
      memcpy(info->user, user, strlen(user));
      char* passwd = strtok(NULL, ",");
      if (!passwd) {
        fprintf(stderr, "illegal form in account file, line %d\n", line);
        continue;
      }
      md5(passwd, strlen(passwd), info->passwd_hash);
      ret = 1;
      break;
    }
  }
  free(buf);
  fclose(file);
  return ret;
}

//handle the login request from client
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
  int succ = 1;
  csiebox_client_info* info =
    (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
    i = info;
  memset(info, 0, sizeof(csiebox_client_info));
  if (!get_account_info(server, login->message.body.user, &(info->account))) {
    fprintf(stderr, "cannot find account\n");
    succ = 0;
  }
  if (succ &&
      memcmp(login->message.body.passwd_hash,
             info->account.passwd_hash,
             MD5_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "passwd miss match\n");
    succ = 0;
  }

  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  header.res.datalen = 0;
  if (succ) {
    if (server->client[conn_fd]) {
      free(server->client[conn_fd]);
    }
    info->conn_fd = conn_fd;
    server->client[conn_fd] = info;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    header.res.client_id = info->conn_fd;
    char* homedir = get_user_homedir(server, info);
    mkdir(homedir, DIR_S_FLAG);
    free(homedir);
  } else {
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    free(info);
  }
  send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
  free(server->client[conn_fd]);
  server->client[conn_fd] = 0;
  close(conn_fd);
}

static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info) {
  char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
  memset(ret, 0, PATH_MAX);
  sprintf(ret, "%s/%s", server->arg.path, info->account.user);
  return ret;
}

