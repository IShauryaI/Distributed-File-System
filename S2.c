#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
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
#include <dirent.h>

//defines how many connections are allowed
#define BACKLOG_20 16

//define maximum length of the text line
#define LINE_MAX_20 4096

//size of our read/write buffer
#define CHUNK_20 8192

//defining default port where S2 listens
//but you can override the port with: .S2/ <port>
static int S2_PORT_20 = 5002;
static const char *HOST_20 = "0.0.0.0";

//this functions makes sure all the bytes are sent to the socket and keeps trying until everything is sent
static ssize_t write_fully_20(int fd_20, const void *buf_20, size_t n_20)
{
    const char *p_20=(const char*)buf_20;
    size_t left_20=n_20;
    while(left_20)
    {
        ssize_t w_20=write(fd_20,p_20,left_20);
        if(w_20<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        left_20-= (size_t)w_20;
        p_20+=w_20;
    }
    return (ssize_t)n_20;
}

//this functions reads bytes
static ssize_t read_fully_20(int fd_20, void *buf_20, size_t n_20)
{
    char *p_20=(char*)buf_20;
    size_t left_20=n_20;
    while(left_20)
    {
        ssize_t r_20=read(fd_20,p_20,left_20);
        if(r_20==0)
            return (ssize_t)(n_20-left_20);

        if(r_20<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        left_20-= (size_t)r_20; p_20+=r_20;
    }
    return (ssize_t)n_20;
}

// this function reads characters until it sees a newline '\n' and gives a clean c string
static int read_line_20(int fd_20, char *buf_20, size_t cap_20)
{
    size_t i_20=0;
    while(i_20+1<cap_20)
    {
        char c_20;
        ssize_t r_20=read(fd_20,&c_20,1);

        if(r_20==0)
            break;
        if(r_20<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        if(c_20=='\n')
            break;
        buf_20[i_20++]=c_20; } buf_20[i_20]='\0';
    return (int)i_20;
}


static int send_line_20(int fd_20, const char *fmt_20, ...)
{
    char buf_20[LINE_MAX_20];
    va_list ap_20;
    va_start(ap_20,fmt_20);
    vsnprintf(buf_20,sizeof buf_20,fmt_20,ap_20);
    va_end(ap_20);
    size_t L_20=strlen(buf_20);
    buf_20[L_20++]='\n';
    return write_fully_20(fd_20,buf_20,L_20)==(ssize_t)L_20?0:-1;
}

//builds the root folder for S2
static char *base_20(void)
{
    const char *home_20=getenv("HOME");
    if(!home_20) home_20=".";
    char *p_20=NULL;
    asprintf(&p_20,"%s/%s",home_20,"S2");
    mkdir(p_20,0700);
    return p_20;
}

//creates absolute path inside S2 if needed
static char *join_20(const char *rel_20)
{
    char *b_20=base_20();
    char *out_20=NULL;
    asprintf(&out_20,"%s/%s",b_20, rel_20 && *rel_20? rel_20:"");
    free(b_20);

    //creates missing folders one by one
    char *tmp_20=strdup(out_20);
    for(char *p_20=tmp_20+1; *p_20; ++p_20)
    {
        if(*p_20=='/')
        {
            *p_20=0;
            mkdir(tmp_20,0700);
            *p_20='/';
        }
    }
    free(tmp_20);
    return out_20;
}

//recieves bytes from the socket and saves the files and also tells S1 that the operations was success
static int do_store_20(int fd_20, char *rel_20, char *name_20, size_t sz_20)
{
    // builds the folder path and full file path
    char *dir_20=join_20(rel_20);
    char *dst_20=NULL;
    asprintf(&dst_20,"%s/%s",dir_20,name_20);
    free(dir_20);

    //create or overwrites the file
    int out_20=open(dst_20,O_CREAT|O_TRUNC|O_WRONLY,0600);
    if(out_20<0)
    {
        free(dst_20);
        return -1;
    }

    //temporary buffer to copy bytes from the socket
    char *buf_20=malloc(CHUNK_20);
    if(!buf_20)
    {
        close(out_20);
        free(dst_20);
        return -1;
    }

    size_t left_20=sz_20;
    while(left_20)
    {
        size_t want_20=left_20>CHUNK_20?CHUNK_20:left_20;
        ssize_t r_20=read_fully_20(fd_20,buf_20,want_20);
        if(r_20<=0)
        {
            free(buf_20);
            close(out_20);
            free(dst_20);
            return -1;
        }
        if(write_fully_20(out_20,buf_20,(size_t)r_20)!=r_20)
        {
            free(buf_20);
            close(out_20);
            free(dst_20);
            return -1;
        }
        left_20-= (size_t)r_20;
    }
    free(buf_20);
    close(out_20);
    free(dst_20);
    send_line_20(fd_20,"OK");
    return 0;
}

//reads a file from the disk and sends it to S1
static int do_fetch_20(int fd_20, char *relfile_20)
{
    char *full_20=join_20(relfile_20);
    int in_20=open(full_20,O_RDONLY);
    if(in_20<0)
    {
        free(full_20);
        return send_line_20(fd_20,"ERR|nofile");
    }

    // finds file so we can calculate the no of bytes
    struct stat st_20; fstat(in_20,&st_20);

    //extract the basename
    const char *base_just_20=strrchr(full_20,'/');
    base_just_20 = base_just_20?base_just_20+1:full_20;
    send_line_20(fd_20,"OK|%s|%zu",base_just_20,(size_t)st_20.st_size);
    char *buf_20=malloc(CHUNK_20);
    for(;;)
    {
        ssize_t r_20=read(in_20,buf_20,CHUNK_20);
        if(r_20<0)
        {
            if(errno==EINTR)
                continue;
            break;
        }
        if(r_20==0)
            break;
        if(write_fully_20(fd_20,buf_20,(size_t)r_20)!=r_20)
            break;
    }
    free(buf_20); close(in_20); free(full_20);
    return 0;
}

//function to delete files from S2 server
static int do_delete_20(char *relfile_20, int fd_20)
{
    char *full_20=join_20(relfile_20);
    int rc_20 = unlink(full_20);
    free(full_20);
    if(rc_20==0)
        return send_line_20(fd_20,"OK");
    return send_line_20(fd_20,"ERR|unlink");
}

//function to create a tar file that contains all .pdfs under S2
static int do_tar_20(int fd_20)
{
    char *b_20=base_20();
    char *tar_20=NULL;
    asprintf(&tar_20,"%s/pdf.tar",b_20);
    char *cmd_20=NULL;
    asprintf(&cmd_20,"cd '%s' && tar -cf '%s' $(find . -type f -name '*.pdf' | sed 's|^\\./||') 2>/dev/null", b_20, tar_20);
    int rc_20=system(cmd_20);
    (void)rc_20;
    free(cmd_20);
    struct stat st_20;

    if(stat(tar_20,&st_20)!=0)
    {
        free(tar_20);
        free(b_20);
        return send_line_20(fd_20,"ERR|empty");
    }
    send_line_20(fd_20,"OK|pdf.tar|%zu",(size_t)st_20.st_size);
    int in_20=open(tar_20,O_RDONLY);
    char *buf_20=malloc(CHUNK_20);
    for(;;)
    {
        ssize_t r_20=read(in_20,buf_20,CHUNK_20);
        if(r_20<=0)
            break;
        write_fully_20(fd_20,buf_20,(size_t)r_20);
    }
    free(buf_20);
    close(in_20);
    unlink(tar_20);
    free(tar_20);
    free(b_20);
    return 0;
}

// sends the name of all the .pdf files under S2 for dispfnames command
static int do_list_20(int fd_20, char *reldir_20)
{
    char *full_20=join_20(reldir_20);
    DIR *d_20=opendir(full_20);
    if(!d_20)
    {
        free(full_20);
        send_line_20(fd_20,"OK");
        send_line_20(fd_20,"END");
        return 0;
    }
    send_line_20(fd_20,"OK");
    struct dirent *e_20;
    while((e_20=readdir(d_20)))
    {
        if(e_20->d_type==DT_REG)
        {
            const char *dot_20=strrchr(e_20->d_name,'.');
            if(dot_20 && strcasecmp(dot_20,".pdf")==0)
            {
                send_line_20(fd_20,"NAME|%s", e_20->d_name);
            }
        }
    }
    closedir(d_20);
    free(full_20);
    send_line_20(fd_20,"END");
    return 0;
}

// handler connected to S1, reads all the commands and calls the correct function for each command
static void serve_20(int cfd_20)
{
    for(;;)
    {
        char line_20[LINE_MAX_20];
        int n_20=read_line_20(cfd_20,line_20,sizeof line_20);
        if(n_20<=0)
            break;
        if(strncmp(line_20,"STORE|",6)==0)
        {
            char *save_20=NULL;
            strtok_r(line_20,"|",&save_20);
            char *rel_20=strtok_r(NULL,"|",&save_20);
            char *name_20=strtok_r(NULL,"|",&save_20);
            char size_line_20[LINE_MAX_20];
            if(read_line_20(cfd_20,size_line_20,sizeof size_line_20)<=0)
                break;
            size_t sz_20=(size_t)strtoull(size_line_20,NULL,10);
            do_store_20(cfd_20, rel_20?rel_20:"", name_20?name_20:"file.pdf", sz_20);
        }
        else if(strncmp(line_20,"FETCH|",6)==0)
        {
            char *relfile_20=line_20+6;
            do_fetch_20(cfd_20, relfile_20);
        }
        else if(strncmp(line_20,"DELETE|",7)==0)
        {
            char *relfile_20=line_20+7;
            do_delete_20(relfile_20, cfd_20);
        }
        else if(strncmp(line_20,"TAR|.pdf",8)==0)
        {
            do_tar_20(cfd_20);
        }
        else if(strncmp(line_20,"LIST|",5)==0)
        {
            do_list_20(cfd_20, line_20+5);
        }
        else
        {
            send_line_20(cfd_20,"ERR|unknown");
        }
    }
}

//cleans up finished child processes so they don't become zombies
static void reap_20(int s)
{
    (void)s;
    while(waitpid(-1,NULL,WNOHANG)>0){}
}

//main()
int main(int argc, char **argv)
{
    if(argc>=2) S2_PORT_20=atoi(argv[1]);
    signal(SIGCHLD,reap_20);

    char *b_20=base_20(); free(b_20);

    int lfd_20=socket(AF_INET,SOCK_STREAM,0);
    int opt_20=1;
    setsockopt(lfd_20,SOL_SOCKET,SO_REUSEADDR,&opt_20,sizeof opt_20);
    struct sockaddr_in a_20;
    memset(&a_20,0,sizeof a_20);
    a_20.sin_family=AF_INET;
    a_20.sin_port=htons((uint16_t)S2_PORT_20);
    inet_pton(AF_INET,HOST_20,&a_20.sin_addr);
    if(bind(lfd_20,(struct sockaddr*)&a_20,sizeof a_20)!=0)
    {
        perror("bind");
        exit(1);
    }

    if(listen(lfd_20,BACKLOG_20)!=0)
    {
        perror("listen");
        exit(1);
    }
    fprintf(stderr,"[S2] listening on %s:%d\n",HOST_20,S2_PORT_20);

    for(;;)
    {
        struct sockaddr_in c_20;
        socklen_t cl_20=sizeof c_20;
        int cfd_20=accept(lfd_20,(struct sockaddr*)&c_20,&cl_20);
        if(cfd_20<0)
        {
            if(errno==EINTR)
                continue;
            perror("accept");
            continue;
        }
        pid_t p_20=fork();
        if(p_20==0)
        {
            close(lfd_20);
            serve_20(cfd_20);
            close(cfd_20);
            _exit(0);
        }
        close(cfd_20);
    }
}
