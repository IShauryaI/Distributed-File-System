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

#define BACKLOG_40 16
#define LINE_MAX_40 4096
#define CHUNK_40 8192

//S4 listens and we can override the port using ./S4 <port>
static int S4_PORT_40 = 5004;
static const char *HOST_40 = "0.0.0.0";

//makes sure we send all the bytes to the socket
static ssize_t write_fully_40(int fd,const void*buf,size_t n)
{
    const char*p=buf; size_t L=n;
    while(L)
    {
        ssize_t w=write(fd,p,L);
        if(w<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        L-=(size_t)w;
        p+=w;
    }
    return (ssize_t)n;
}

//reads up to N bytes and stops early if connection is closed
static ssize_t read_fully_40(int fd,void*buf,size_t n)
{
    char*p=buf;
    size_t L=n;
    while(L)
    {
        ssize_t r=read(fd,p,L);
        if(r==0)
            return (ssize_t)(n-L);
        if(r<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        L-=(size_t)r;
        p+=r;
    }
    return (ssize_t)n;
}

//reads characters one by one until we hit newline '\n' the we remove the newline, so the caller gets a clean string
static int read_line_40(int fd,char*b,size_t cap)
{
    size_t i=0;
    while(i+1<cap)
    {
        char c;
        ssize_t r=read(fd,&c,1);
        if(r==0)
            break;
        if(r<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        } if(c=='\n')
            break;
        b[i++]=c;
    }
    b[i]='\0';
    return (int)i;
}

static int send_line_40(int fd, const char*fmt, ...)
{
    char buf[LINE_MAX_40];
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(buf,sizeof buf,fmt,ap);
    va_end(ap); size_t L=strlen(buf);
    buf[L++]='\n';
    return write_fully_40(fd,buf,L)==(ssize_t)L?0:-1; }

//ensures that S4 exists
static char *base_40(void)
{
    const char*home=getenv("HOME");
    if(!home) home=".";
    char *p=NULL;
    asprintf(&p,"%s/%s",home,"S4");
    mkdir(p,0700); return p;
}

//turns relative path to absolute path and creates missing folders
static char *join_40(const char *rel)
{
    char *b=base_40();
    char *o=NULL;
    asprintf(&o,"%s/%s",b,rel?rel:"");
    free(b);
    char *t=strdup(o);
    for(char *q=t+1; *q; ++q)
    {
        if(*q=='/')
        {
            *q=0;
            mkdir(t,0700); *q='/';
        }
    } free(t); return o;
}

//receives files from S1 and saves them
static int do_store_40(int fd, char*rel, char*name, size_t sz)
{
    char *dir=join_40(rel);
    char *dst=NULL;
    asprintf(&dst,"%s/%s",dir,name);
    free(dir);
    int out=open(dst,O_CREAT|O_TRUNC|O_WRONLY,0600);
    if(out<0)
    {
        free(dst);
        return -1;
    }
    char *buf=malloc(CHUNK_40);
    size_t left=sz;
    while(left)
    {
        size_t want=left>CHUNK_40?CHUNK_40:left;
        ssize_t r=read_fully_40(fd,buf,want);
        if(r<=0)
        {
            free(buf);
            close(out);
            free(dst);
            return -1;
        }
        if (write_fully_40(out, buf, (size_t)r) != r)
        {
            free(buf);
            close(out);
            free(dst);
            return -1;
        }
        left-=(size_t)r;
    }
    free(buf);
    close(out);
    free(dst);
    send_line_40(fd,"OK");
    return 0;
}

//sead a file from disk and send it back to S1
static int do_fetch_40(int fd, char *relfile)
{
    char *full=join_40(relfile);
    int in=open(full,O_RDONLY);
    if(in<0)
    {
        free(full);
        return send_line_40(fd,"ERR|nofile");
    }
    struct stat st;
    fstat(in,&st);
    const char *bn=strrchr(full,'/');
    bn=bn?bn+1:full;
    send_line_40(fd,"OK|%s|%zu",bn,(size_t)st.st_size);
    char *buf=malloc(CHUNK_40);
    for(;;)
    {
        ssize_t r=read(in,buf,CHUNK_40);
        if(r<=0)
            break;
        write_fully_40(fd,buf,(size_t)r);
    }
    free(buf);
    close(in);
    free(full);
    return 0;
}

//deletes a file under S4
static int do_delete_40(char *relfile, int fd)
{
    char *full=join_40(relfile);
    int rc=unlink(full);
    free(full);
    return send_line_40(fd, rc==0? "OK":"ERR|unlink");
}

//lists the name of all the .zip files under S4
static int do_list_40(int fd, char *reldir)
{
    char *full=join_40(reldir);
    DIR *d=opendir(full);
    if(!d)
    {
        free(full);
        send_line_40(fd,"OK");
        send_line_40(fd,"END");
        return 0;
    }
    send_line_40(fd,"OK");
    struct dirent *e;
    while((e=readdir(d)))
    {
        if(e->d_type==DT_REG)
        {
            const char *dot=strrchr(e->d_name,'.');
            if(dot && strcasecmp(dot,".zip")==0)
                send_line_40(fd,"NAME|%s", e->d_name);
        }
    }
    closedir(d); free(full);
    send_line_40(fd,"END");
    return 0;
}

//handles connection from S1 and calls the right function for each command.
static void serve_40(int cfd)
{
    for(;;)
    {
        char line[LINE_MAX_40];
        int n=read_line_40(cfd,line,sizeof line);
        if(n<=0)
            break;
        if(strncmp(line,"STORE|",6)==0)
        {
            char *sv=NULL; strtok_r(line,"|",&sv);
            char *rel=strtok_r(NULL,"|",&sv);
            char *name=strtok_r(NULL,"|",&sv);
            char szl[LINE_MAX_40];

            if(read_line_40(cfd,szl,sizeof szl)<=0)
                break;
            size_t sz=(size_t)strtoull(szl,NULL,10);
            do_store_40(cfd, rel?rel:"", name?name:"file.zip", sz);
        }
        else if(strncmp(line,"FETCH|",6)==0)
        {
            do_fetch_40(cfd, line+6);
        }
        else if(strncmp(line,"DELETE|",7)==0)
        {
            do_delete_40(line+7, cfd);
        }
        else if(strncmp(line,"LIST|",5)==0)
        {
            do_list_40(cfd, line+5);
        }
        else
        {
            send_line_40(cfd,"ERR|unknown");
        }
    }
}

//cleans up finished child processes
static void reap_40(int s)
{
    (void)s;
    while(waitpid(-1,NULL,WNOHANG)>0){}
}

//main() starts the server S4 and performs the other functions
int main(int argc,char**argv)
{
    if(argc>=2)
        S4_PORT_40=atoi(argv[1]);

    signal(SIGCHLD,reap_40);

    char *b=base_40(); free(b);
    int lfd=socket(AF_INET,SOCK_STREAM,0);
    int opt=1; setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
    struct sockaddr_in a;
    memset(&a,0,sizeof a);
    a.sin_family=AF_INET;
    a.sin_port=htons((uint16_t)S4_PORT_40);
    inet_pton(AF_INET,HOST_40,&a.sin_addr);

    if(bind(lfd,(struct sockaddr*)&a,sizeof a)!=0)
    {
        perror("bind");
        exit(1);
    }
    if(listen(lfd,BACKLOG_40)!=0)
    {
        perror("listen");
        exit(1);
    }
    fprintf(stderr,"[S4] listening on %s:%d\n",HOST_40,S4_PORT_40);
    for(;;)
    {
        struct sockaddr_in c;
        socklen_t cl=sizeof c;
        int cfd=accept(lfd,(struct sockaddr*)&c,&cl);
        if(cfd<0)
        {
            if(errno==EINTR)
                continue;
            perror("accept");
            continue;
        }
        pid_t p=fork();
        if(p==0)
        {
            close(lfd);
            serve_40(cfd);
            close(cfd);
            _exit(0);
        }
        close(cfd);
    }
}
