#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG_30 16
#define LINE_MAX_30 4096
#define CHUNK_30 8192

//S3 listens and you can override the port: ./S3<port>
static int S3_PORT_30 = 5003;
static const char *HOST_30 = "0.0.0.0";

//sends all the bytes to a file/socket
static ssize_t write_fully_30(int fd_30,const void*buf_30,size_t n_30)
{
    const char*p_30=buf_30;
    size_t L_30=n_30;
    while(L_30)
    {
        ssize_t w_30=write(fd_30,p_30,L_30);
        if(w_30<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        L_30-=(size_t)w_30;
        p_30+=w_30;
    }
    return (ssize_t)n_30;
}

//reads up to N bytes and stops early if connection is closed (r_30 == 0)
static ssize_t read_fully_30(int fd_30,void*buf_30,size_t n_30)
{
    char*p_30=buf_30;
    size_t L_30=n_30;
    while(L_30)
    {
        ssize_t r_30=read(fd_30,p_30,L_30);
        if(r_30==0)
            return (ssize_t)(n_30-L_30);
        if(r_30<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        L_30-=(size_t)r_30;
        p_30+=r_30;
    }
    return (ssize_t)n_30;
}

//read characters until we see a newline '\n' then we remove the newline and return a clean C string.
static int read_line_30(int fd_30,char*buf_30,size_t cap_30)
{
    size_t i_30=0;
    while(i_30+1<cap_30)
    { char c_30;
        ssize_t r_30=read(fd_30,&c_30,1);
        if(r_30==0)
            break;
        if(r_30<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        if(c_30=='\n')
            break;
        buf_30[i_30++]=c_30;
    }
    buf_30[i_30]='\0';
    return (int)i_30;
}

static int send_line_30(int fd_30, const char*fmt_30, ...)
{
    char b_30[LINE_MAX_30];
    va_list ap_30;
    va_start(ap_30,fmt_30);
    vsnprintf(b_30,sizeof b_30,fmt_30,ap_30);
    va_end(ap_30);
    size_t L_30=strlen(b_30);
    b_30[L_30++]='\n';
    return write_fully_30(fd_30,b_30,L_30)==(ssize_t)L_30?0:-1;
}

//checks if S3 exists in the directory
static char *base_30(void)
{
    const char*home_30=getenv("HOME");
    if(!home_30) home_30=".";
    char *p_30=NULL;
    asprintf(&p_30,"%s/%s",home_30,"S3");
    mkdir(p_30,0700); return p_30;
}

//turns relative path to absolute path and creates missing folders
static char *join_30(const char *rel_30)
{
    char *b_30=base_30();
    char *o_30=NULL;
    asprintf(&o_30,"%s/%s",b_30,rel_30?rel_30:"");
    free(b_30);
    char *t_30=strdup(o_30);
    for(char *p=t_30+1; *p; ++p)
    {
        if(*p=='/')
        {
            *p=0;
            mkdir(t_30,0700);
            *p='/';
        }
    }
    free(t_30);
    return o_30;
}

//recieves files from S1 and saves them
static int do_store_30(int fd_30, char*rel_30, char*name_30, size_t sz_30)
{
    char *dir_30=join_30(rel_30);
    char *dst_30=NULL;
    asprintf(&dst_30,"%s/%s",dir_30,name_30);
    free(dir_30);
    int out_30=open(dst_30,O_CREAT|O_TRUNC|O_WRONLY,0600);
    if(out_30<0)
    {
        free(dst_30);
        return -1;
    }
    char *buf_30=malloc(CHUNK_30);
    size_t left_30=sz_30;

    while(left_30)
    {
        size_t want_30=left_30>CHUNK_30?CHUNK_30:left_30;
        ssize_t r_30=read_fully_30(fd_30,buf_30,want_30);
        if(r_30<=0)
        {
            free(buf_30);
            close(out_30);
            free(dst_30);
            return -1;
        }
        if(write_fully_30(out_30,buf_30,(size_t)r_30)!=r_30)
        {
            free(buf_30);
            close(out_30);
            free(dst_30);
            return -1;
        }
        left_30-=(size_t)r_30;
    }
    free(buf_30);
    close(out_30);
    free(dst_30);
    send_line_30(fd_30,"OK");
    return 0;
}

//send a file back to S1 if required
static int do_fetch_30(int fd_30, char *relfile_30)
{
    char *full_30=join_30(relfile_30);
    int in_30=open(full_30,O_RDONLY);
    if(in_30<0)
    {
        free(full_30);
        return send_line_30(fd_30,"ERR|nofile");
    }
    struct stat st_30;
    fstat(in_30,&st_30);
    const char *bn_30=strrchr(full_30,'/');
    bn_30=bn_30?bn_30+1:full_30;
    send_line_30(fd_30,"OK|%s|%zu",bn_30,(size_t)st_30.st_size);
    char *buf_30=malloc(CHUNK_30);
    for(;;)
    {
        ssize_t r_30=read(in_30,buf_30,CHUNK_30);
        if(r_30<=0)
            break;
        write_fully_30(fd_30,buf_30,(size_t)r_30);
    }
    free(buf_30);
    close(in_30);
    free(full_30);
    return 0;
}

//removes files from S3 if required
static int do_delete_30(char *relfile_30, int fd_30)
{
    char *full_30=join_30(relfile_30);
    int rc_30=unlink(full_30);
    free(full_30);
    return send_line_30(fd_30, rc_30==0? "OK":"ERR|unlink");
}

//created tar file for all .txt files under S3
static int do_tar_30(int fd_30)
{
    char *b_30=base_30();
    char *tar_30=NULL;
    asprintf(&tar_30,"%s/text.tar",b_30);
    char *cmd_30=NULL;
    asprintf(&cmd_30,"cd '%s' && tar -cf '%s' $(find . -type f -name '*.txt' | sed 's|^\\./||') 2>/dev/null", b_30, tar_30);
    int rc_30=system(cmd_30);
    (void)rc_30; free(cmd_30);
    struct stat st_30;
    if(stat(tar_30,&st_30)!=0)
    {
        free(tar_30);
        free(b_30);
        return send_line_30(fd_30,"ERR|empty");
    }
    send_line_30(fd_30,"OK|text.tar|%zu",(size_t)st_30.st_size);
    int in_30=open(tar_30,O_RDONLY);
    char *buf_30=malloc(CHUNK_30);
    for(;;)
    {
        ssize_t r_30=read(in_30,buf_30,CHUNK_30);
        if(r_30<=0)
            break;
        write_fully_30(fd_30,buf_30,(size_t)r_30);
    }
    free(buf_30);
    close(in_30);
    unlink(tar_30);
    free(tar_30);
    free(b_30);
    return 0;
}

//list the name of all the txt files under S3
static int do_list_30(int fd_30, char *reldir_30)
{
    char *full_30=join_30(reldir_30);
    DIR *d_30=opendir(full_30);
    if(!d_30)
    {
        free(full_30);
        send_line_30(fd_30,"OK");
        send_line_30(fd_30,"END");
        return 0;
    }
    send_line_30(fd_30,"OK");
    struct dirent *e_30;
    while((e_30=readdir(d_30)))
    {
        if(e_30->d_type==DT_REG)
        {
            const char *dot_30=strrchr(e_30->d_name,'.');
            if(dot_30 && strcasecmp(dot_30,".txt")==0)
                send_line_30(fd_30,"NAME|%s", e_30->d_name);
        }
    }
    closedir(d_30); free(full_30);
    send_line_30(fd_30,"END");
    return 0;
}

//handles connection from S1 and call right handler for each function
static void serve_30(int cfd_30)
{
    for(;;)
    {
        char line_30[LINE_MAX_30];
        int n_30=read_line_30(cfd_30,line_30,sizeof line_30);
        if(n_30<=0)
            break;
        if(strncmp(line_30,"STORE|",6)==0)
        {
            char *sv_30=NULL; strtok_r(line_30,"|",&sv_30);
            char *rel_30=strtok_r(NULL,"|",&sv_30);
            char *name_30=strtok_r(NULL,"|",&sv_30);
            char szl_30[LINE_MAX_30];
            if(read_line_30(cfd_30,szl_30,sizeof szl_30)<=0)
                break;
            size_t sz_30=(size_t)strtoull(szl_30,NULL,10);
            do_store_30(cfd_30, rel_30?rel_30:"", name_30?name_30:"file.txt", sz_30);
        }
        else if(strncmp(line_30,"FETCH|",6)==0)
        {
            do_fetch_30(cfd_30, line_30+6);
        }
        else if(strncmp(line_30,"DELETE|",7)==0)
        {
            do_delete_30(line_30+7, cfd_30);
        }
        else if(strncmp(line_30,"TAR|.txt",8)==0)
        {
            do_tar_30(cfd_30);
        }
        else if(strncmp(line_30,"LIST|",5)==0)
        {
            do_list_30(cfd_30, line_30+5);
        }
        else
        {
            send_line_30(cfd_30,"ERR|unknown");
        }
    }
}

//cleans up the child processes
static void reap_30(int s)
{
    (void)s;
    while(waitpid(-1,NULL,WNOHANG)>0){}
}

//main () and starts the server S3 and performs the other functions
int main(int argc,char**argv)
{
    if(argc>=2)
        S3_PORT_30=atoi(argv[1]);
    signal(SIGCHLD,reap_30);
    char *b_30=base_30();
    free(b_30);

    int lfd_30=socket(AF_INET,SOCK_STREAM,0);
    int opt_30=1;
    setsockopt(lfd_30,SOL_SOCKET,SO_REUSEADDR,&opt_30,sizeof opt_30);
    struct sockaddr_in a_30;
    memset(&a_30,0,sizeof a_30);
    a_30.sin_family=AF_INET;
    a_30.sin_port=htons((uint16_t)S3_PORT_30);
    inet_pton(AF_INET,HOST_30,&a_30.sin_addr);

    if(bind(lfd_30,(struct sockaddr*)&a_30,sizeof a_30)!=0)
    {
        perror("bind");
        exit(1);
    }
    if(listen(lfd_30,BACKLOG_30)!=0)
    {
        perror("listen");
        exit(1);
    }
    fprintf(stderr,"[S3] listening on %s:%d\n",HOST_30,S3_PORT_30);

    for(;;)
    {
        struct sockaddr_in c_30;
        socklen_t cl_30=sizeof c_30;
        int cfd_30=accept(lfd_30,(struct sockaddr*)&c_30,&cl_30);
        if(cfd_30<0)
        {
            if(errno==EINTR)
                continue;
            perror("accept");
            continue;
        }
        pid_t p_30=fork();
        if(p_30==0)
        {
            close(lfd_30);
            serve_30(cfd_30);
            close(cfd_30);
            _exit(0);
        }
        close(cfd_30);
    }
}
