/*HOW TO RUN
    - Normal:      ./s25client 127.0.0.1 5001
    - Custom host: ./s25client <S1_HOST> <S1_PORT>
   ===================================================================== */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// maximum lenght for a line
#define LINE_MAX_50 4096

//max buffer size
#define CHUNK_50 8192

//global target S1
static const char *S1_HOST_50 = "127.0.0.1";

//default port but we can override it
static int S1_PORT_50 = 5001;

//I/O helpers

//send exactly n_50 bytes to the socket
static ssize_t write_fully_50(int fd_50, const void *buf_50, size_t n_50)
{
    const char *p_50=(const char*)buf_50; size_t left_50=n_50;
    while(left_50>0)
    {
        ssize_t w_50=write(fd_50,p_50,left_50);
        if(w_50<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        left_50 -= (size_t)w_50;
        p_50 += w_50;
    }
    return (ssize_t)n_50;
}

//reads bytes upto n_50
static ssize_t read_fully_50(int fd_50, void *buf_50, size_t n_50)
{
    char *p_50=(char*)buf_50;
    size_t left_50=n_50;
    while(left_50>0)
    {
        ssize_t r_50=read(fd_50,p_50,left_50);
        if(r_50==0)
            return (ssize_t)(n_50-left_50);
        if(r_50<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        left_50 -= (size_t)r_50;
        p_50 += r_50;
    }
    return (ssize_t)n_50;
}

//reads one text line ending with '\n' and removes that newline
static int read_line_50(int fd_50, char *buf_50, size_t cap_50)
{
    size_t i_50=0;
    while(i_50+1<cap_50)
    {
        char c_50;
        ssize_t r_50=read(fd_50,&c_50,1);
        if(r_50==0)
            break;
        if(r_50<0)
        {
            if(errno==EINTR)
                continue;
            return -1;
        }
        if(c_50=='\n')
            break;
        buf_50[i_50++]=c_50;
    }
    buf_50[i_50]='\0';
    return (int)i_50;
}

static int send_line_50(int fd_50, const char *fmt_50, ...)
{
    char line_50[LINE_MAX_50];
    va_list ap_50; va_start(ap_50,fmt_50);
    vsnprintf(line_50,sizeof line_50,fmt_50,ap_50);
    va_end(ap_50);
    size_t L_50=strlen(line_50);
    if(L_50+1>=sizeof line_50)
        return -1;
    line_50[L_50++] = '\n';
    return (write_fully_50(fd_50,line_50,L_50)==(ssize_t)L_50)?0:-1;
}

//connects to S1 using its port
static int connect_s1_50(void)
{
    int fd_50=socket(AF_INET,SOCK_STREAM,0);
    if(fd_50<0)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in a_50;
    memset(&a_50,0,sizeof a_50);
    a_50.sin_family=AF_INET;
    a_50.sin_port=htons((uint16_t)S1_PORT_50);
    if(inet_pton(AF_INET,S1_HOST_50,&a_50.sin_addr)!=1)
    {
        fprintf(stderr,"bad S1 host\n");
        close(fd_50);
        return -1;
    }
    if(connect(fd_50,(struct sockaddr*)&a_50,sizeof a_50)!=0)
    {
        perror("connect");
        close(fd_50);
        return -1;
    }
    return fd_50;
}

//helpers for paths and files

//checks if a string starts with "~S1/"
static int path_is_s1_50(const char *p_50)
{
    return p_50 && strncmp(p_50,"~S1/",4)==0;
}

//returns only the file name part from a path
static const char *basename_50(const char *p_50)
{
    const char *s=strrchr(p_50,'/');
    return s? s+1 : p_50;
}

//returns file size of a local file
static int get_size_50(const char *path_50, size_t *out_50)
{
    struct stat st_50;
    if(stat(path_50,&st_50)!=0 || !S_ISREG(st_50.st_mode))
        return -1;
    *out_50=(size_t)st_50.st_size;
    return 0;
}

//sends a local file's bytes to S1
static int send_file_50(int fd_50, const char *path_50)
{
    int in_50=open(path_50,O_RDONLY);
    if(in_50<0)
        return -1;
    char *buf_50 = malloc(CHUNK_50);
    if(!buf_50)
    {
        close(in_50);
        return -1;
    }
    for(;;)
    {
        ssize_t r_50=read(in_50,buf_50,CHUNK_50);
        if(r_50<0)
        {
            if(errno==EINTR)
                continue;
            free(buf_50);
            close(in_50);
            return -1;
        }
        if(r_50==0)
            break;
        if(write_fully_50(fd_50,buf_50,(size_t)r_50)!=r_50)
        {
            free(buf_50);
            close(in_50);
            return -1;
        }
    }
    free(buf_50);
    close(in_50);
    return 0;
}

//saves bytes from S1 into a local file
static int recv_file_50(int fd_50, const char *out_50, size_t sz_50)
{
    int outfd_50=open(out_50,O_CREAT|O_TRUNC|O_WRONLY,0600);
    if(outfd_50<0)
    {
        perror("open out");
        return -1;
    }
    char *buf_50 = malloc(CHUNK_50);
    if(!buf_50)
    {
        close(outfd_50);
        return -1;
    }
    size_t left_50=sz_50;
    while(left_50>0)
    {
        size_t want_50 = left_50>CHUNK_50 ? CHUNK_50 : left_50;
        ssize_t r_50 = read_fully_50(fd_50,buf_50,want_50);
        if(r_50<=0)
        {
            free(buf_50);
            close(outfd_50);
            return -1;
        }
        if(write_fully_50(outfd_50,buf_50,(size_t)r_50)!=r_50)
        {
            free(buf_50);
            close(outfd_50);
            return -1;
        }
        left_50 -= (size_t)r_50;
    }
    free(buf_50); close(outfd_50); return 0;
}

// for uploadf --------------------
//check if there are more than 3 args and connects to S1 and uploads the file
static void cmd_uploadf_50(int argc_50, char **argv_50)
{
    if(argc_50>5)
    {
        fprintf(stderr,"only 5 arguments are allowed\n");
        return;
    }
    if(argc_50<3)
    {
        fprintf(stderr,"usage: uploadf file1 file2 file3 ~S1/...\n");
        return;
    }
    const char *dest_50 = argv_50[argc_50-1];
    int files_n_50 = argc_50-2;
    if(!path_is_s1_50(dest_50) || files_n_50<1 || files_n_50>3)
    {
        fprintf(stderr,"usage: uploadf file1 file2 file3 ~S1/...\n");
        return;
    }

    //gets sizes first so we can tell S1 how much we will send
    size_t sizes_50[3]={0,0,0};
    for(int i_50=0;i_50<files_n_50;i_50++)
    {
        if(get_size_50(argv_50[1+i_50], &sizes_50[i_50])!=0)
        {
            fprintf(stderr,"uploadf: cannot read %s\n", argv_50[1+i_50]);
            return;
        }
    }
    int fd_50=connect_s1_50();
    if(fd_50<0)
        return;
    if(send_line_50(fd_50,"UPLOADF|%d|%s", files_n_50, dest_50)!=0)
    {
        fprintf(stderr,"uploadf: send header failed\n");
        close(fd_50);
        return;
    }
    for(int i_50=0;i_50<files_n_50;i_50++)
    {
        const char *path_50 = argv_50[1+i_50];
        const char *name_50 = basename_50(path_50);
        if(send_line_50(fd_50,"FILEMETA|%s|%zu", name_50, sizes_50[i_50])!=0)
        {
            fprintf(stderr,"uploadf: send meta failed\n");
            close(fd_50);
            return;
        }
        if(send_file_50(fd_50,path_50)!=0)
        {
            fprintf(stderr,"uploadf: send data failed on %s\n", name_50);
            close(fd_50);
            return;
        }
    }
    char line_50[LINE_MAX_50];
    int r_50=read_line_50(fd_50,line_50,sizeof line_50);
    if(r_50<=0)
    {
        fprintf(stderr,"uploadf: no server reply\n");
        close(fd_50);
        return;
    }
    if(!strncmp(line_50,"OK",2))
    {
        printf("files uploaded successfully \n");
    }
    else
    {
        printf("%s\n", line_50);
    }
    close(fd_50);
}

//for downlf --------------------
// checks args 2 and 3, then asks S1 for those files and downloads them
static void cmd_downlf_50(int argc_50, char **argv_50)
{
    if(argc_50<2 || argc_50>3 || !path_is_s1_50(argv_50[1]) || (argc_50==3 && !path_is_s1_50(argv_50[2])))
    {
        fprintf(stderr,"usage: downlf ~S1/file1 ~S1/file2\n");
        return;
    }
    int fd_50=connect_s1_50();
    if(fd_50<0)
        return;

    if(argc_50==2)
        send_line_50(fd_50,"DOWNLF|1|%s", argv_50[1]);
    else
        send_line_50(fd_50,"DOWNLF|2|%s|%s", argv_50[1], argv_50[2]);

    int need_50 = argc_50-1;
    int got_ok_50 = 0;
    for(;;)
    {
        char line_50[LINE_MAX_50];
        int n_50=read_line_50(fd_50,line_50,sizeof line_50);
        if(n_50<=0)
            break;

        if(!strncmp(line_50,"FILERESP|",9))
        {
            char *save_50=NULL;
            char *name_50 = strtok_r(line_50+9,"|",&save_50);
            char *szs_50  = strtok_r(NULL,"|",&save_50);
            size_t sz_50 = (size_t)strtoull(szs_50?szs_50:"0",NULL,10);
            if(!name_50)
            {
                fprintf(stderr,"downlf: bad header\n");
                break;
            }
            if(recv_file_50(fd_50, name_50, sz_50)!=0)
            {
                fprintf(stderr,"downlf: receive failed for %s\n", name_50);
                break;
            }
            printf("Downloaded %s (%zu bytes)\n", name_50, sz_50);
            got_ok_50++;
            if(got_ok_50==need_50)
            {
                /* optional DONE */
                char tmp_50[LINE_MAX_50];
                read_line_50(fd_50,tmp_50,sizeof tmp_50);
                break;
            }
        }
        else if(!strncmp(line_50,"FILENOTFOUND|",13))
        {
            printf("%s\n", line_50);
        }
        else if(!strcmp(line_50,"DONE"))
        {
            break;
        }
        else
        {
            printf("%s\n", line_50);
            break;
        }
    }
    close(fd_50);
    if(got_ok_50==need_50)
        printf("files downloaded successfully \n");
}

//for removef --------------------
// checks args 1 and 2 and asks S1 to delete those files
static void cmd_removef_50(int argc_50, char **argv_50)
{
    if(argc_50<2 || argc_50>3 || !path_is_s1_50(argv_50[1]) || (argc_50==3 && !path_is_s1_50(argv_50[2])))
    {
        fprintf(stderr,"usage: removef ~S1/file1 ~S1/file2\n");
        return;
    }
    int fd_50=connect_s1_50();
    if(fd_50<0)
        return;

    int need_50 = argc_50-1;
    if(argc_50==2)
        send_line_50(fd_50,"REMOVEF|1|%s", argv_50[1]);
    else
        send_line_50(fd_50,"REMOVEF|2|%s|%s", argv_50[1], argv_50[2]);

    int got_50 = 0;
    while(got_50 < need_50)
    {
        char line_50[LINE_MAX_50];
        int r_50=read_line_50(fd_50,line_50,sizeof line_50);
        if(r_50<=0)
            break;
        if(!strncmp(line_50,"REMOK|",6))
        {
            const char *path_50 = line_50+6;
            printf("removed file %s\n", basename_50(path_50));
            got_50++;
        }
        else if(!strncmp(line_50,"REMERR|",7))
        {
            /* REMERR|path|reason */
            char *save_50=NULL;
            char *path_50 = strtok_r(line_50+7,"|",&save_50);
            char *why_50  = strtok_r(NULL,"|",&save_50);
            printf("failed to remove %s: %s\n", basename_50(path_50?path_50:"(unknown)"), why_50?why_50:"error");
            got_50++;
        }
        else
        {
            printf("%s\n", line_50);
            break;
        }
    }
    close(fd_50);
}

//downltar --------------------
// request .c, .pdf and .txt files and for each type, we create a new connection to S1 and save the downloaded tar files
static int is_valid_type_50(const char *t_50)
{
    return (!strcmp(t_50,".c") || !strcmp(t_50,".pdf") || !strcmp(t_50,".txt"));
}
static void push_type_50(const char *t_50, const char **arr_50, int *n_50)
{
    if(*n_50>=3)
        return;
    for(int i=0;i<*n_50;i++)
        if(!strcmp(arr_50[i],t_50))
            return;
    arr_50[(*n_50)++] = t_50;
}
static int do_one_downltar_50(const char *type_50)
{
    int fd_50=connect_s1_50();
    if(fd_50<0)
        return -1;
    if(send_line_50(fd_50,"DOWNTAR|%s", type_50)!=0)
    {
        close(fd_50);
        return -1;
    }

    char line_50[LINE_MAX_50];
    int n_50=read_line_50(fd_50,line_50,sizeof line_50);
    if(n_50<=0)
    {
        fprintf(stderr,"downltar: no reply for %s\n", type_50);
        close(fd_50);
        return -1;
    }

    if(!strncmp(line_50,"FILERESP|",9))
    {
        char *save_50=NULL;
        char *name_50 = strtok_r(line_50+9,"|",&save_50);
        char *szs_50  = strtok_r(NULL,"|",&save_50);
        size_t sz_50 = (size_t)strtoull(szs_50?szs_50:"0",NULL,10);
        if(!name_50)
        {
            fprintf(stderr,"downltar: bad header for %s\n", type_50);
            close(fd_50);
            return -1;
        }
        if(recv_file_50(fd_50, name_50, sz_50)!=0)
        {
            fprintf(stderr,"downltar: receive failed for %s\n", type_50);
            close(fd_50);
            return -1;
        }
        printf("Downloaded %s (%zu bytes)\n", name_50, sz_50);
    }
    else
    {
        printf("%s\n", line_50);
        close(fd_50);
        return -1;
    }
    close(fd_50);
    return 0;
}

static void cmd_downltar_50(int argc_50, char **argv_50)
{
    const char *want_50[3];
    int ntypes_50=0;

    if(argc_50<2)
    {
        fprintf(stderr,"usage: downltar .c or .pdf or .txt\n");
        return;
    }
    if(argc_50==2)
    {
        if(!strcmp(argv_50[1],"all"))
        {
            push_type_50(".c", want_50, &ntypes_50);
            push_type_50(".pdf", want_50, &ntypes_50);
            push_type_50(".txt", want_50, &ntypes_50);
        }
        else if(strchr(argv_50[1],'|') || strchr(argv_50[1],','))
        {
            char *tmp_50=strdup(argv_50[1]);
            for(char *tok_50=strtok(tmp_50,"|,");
            tok_50; tok_50=strtok(NULL,"|,"))
            {
                if(is_valid_type_50(tok_50))
                    push_type_50(tok_50, want_50, &ntypes_50);
            }
            free(tmp_50);
        }
        else if(is_valid_type_50(argv_50[1]))
        {
            push_type_50(argv_50[1], want_50, &ntypes_50);
        }
    }
    else
    {
        for(int i=1;i<argc_50;i++)
        {
            if(!strcmp(argv_50[i],"all"))
            {
                push_type_50(".c", want_50, &ntypes_50);
                push_type_50(".pdf", want_50, &ntypes_50);
                push_type_50(".txt", want_50, &ntypes_50);
            }
            else if(is_valid_type_50(argv_50[i]))
            {
                push_type_50(argv_50[i], want_50, &ntypes_50);
            }
        }
    }
    if(ntypes_50==0)
    {
        fprintf(stderr,"usage: downltar .c or .pdf or .txt\n");
        return;
    }
    for(int i=0;i<ntypes_50;i++)
        (void)do_one_downltar_50(want_50[i]);
}

//dispfnames--------------------

//asks S1 for the list of files under each directory and lists them
static void cmd_dispfnames_50(int argc_50, char **argv_50)
{
    if(argc_50!=2 || !path_is_s1_50(argv_50[1]))
    {
        fprintf(stderr,"usage: dispfnames ~S1/dir\n");
        return;
    }
    int fd_50=connect_s1_50();
    if(fd_50<0)
        return;
    if(send_line_50(fd_50,"DISP|%s", argv_50[1])!=0)
    {
        close(fd_50);
        return;
    }

    /* Collect grouped names */
    char **cvec=NULL, **pvec=NULL, **tvec=NULL, **zvec=NULL;
    int cc=0, pc=0, tc=0, zc=0;
    int ca=8, pa=8, ta=8, za=8;
    cvec = malloc(sizeof(char*)*ca);
    pvec = malloc(sizeof(char*)*pa);
    tvec = malloc(sizeof(char*)*ta);
    zvec = malloc(sizeof(char*)*za);

    int seen_begin_50=0;
    char line_50[LINE_MAX_50];
    for(;;)
    {
        int n_50=read_line_50(fd_50,line_50,sizeof line_50);
        if(n_50<=0)
            break;
        if(!seen_begin_50)
        {
            if(!strcmp(line_50,"LISTBEGIN"))
            {
                seen_begin_50=1;
                continue;
            }
            if(!strncmp(line_50,"ERR|",4))
            {
                printf("%s\n", line_50);
                break;
            }
            continue;
        }
        if(!strcmp(line_50,"LISTEND"))
            break;
        if(!strncmp(line_50,"NAME|",5))
        {
            char *save=NULL;
            char *ext = strtok_r(line_50+5,"|",&save);
            char *nm  = strtok_r(NULL,"|",&save);
            if(!ext || !nm)
                continue;
            if(!strcmp(ext,".c"))
            {
                if(cc==ca)
                {
                    ca*=2;
                    cvec=realloc(cvec,sizeof(char*)*ca);
                }
                cvec[cc++]=strdup(nm);
            }
            else if(!strcmp(ext,".pdf"))
            {
                if(pc==pa)
                {
                    pa*=2;
                    pvec=realloc(pvec,sizeof(char*)*pa);
                }
                pvec[pc++]=strdup(nm);
            }
            else if(!strcmp(ext,".txt"))
            {
                if(tc==ta)
                {
                    ta*=2;
                    tvec=realloc(tvec,sizeof(char*)*ta);
                }
                tvec[tc++]=strdup(nm);
            }
            else if(!strcmp(ext,".zip"))
            {
                if(zc==za)
                { za*=2;
                    zvec=realloc(zvec,sizeof(char*)*za);
                }
                zvec[zc++]=strdup(nm);
            }
        }
    }
    close(fd_50);

    // printing the files in order
    if(cc>0)
    {
        printf(".c files\n");
        for(int i=0;i<cc;i++)
            printf("%s\n", cvec[i]);
        printf("\n");
    }
    if(pc>0)
    {
        printf(".pdf files\n");
        for(int i=0;i<pc;i++)
            printf("%s\n", pvec[i]);
        printf("\n");
    }
    if(tc>0)
    {
        printf(".txt files\n");
        for(int i=0;i<tc;i++)
            printf("%s\n", tvec[i]);
        printf("\n");
    }
    if(zc>0)
    {
        printf(".zip files\n");
        for(int i=0;i<zc;i++)
            printf("%s\n", zvec[i]);
        printf("\n");
    }

    for(int i=0;i<cc;i++)
        free(cvec[i]);
    free(cvec);
    for(int i=0;i<pc;i++)
        free(pvec[i]);
    free(pvec);
    for(int i=0;i<tc;i++)
        free(tvec[i]);
    free(tvec);
    for(int i=0;i<zc;i++)
        free(zvec[i]);
    free(zvec);
}

//splits a command to separate them so that we can work on them separately
static int parse_line_50(char *line_50, char **argv_50, int maxv_50)
{
    int n_50=0;
    for(char *p=line_50; *p && n_50<maxv_50; )
    {
        while(*p && (*p==' '||*p=='\t'||*p=='\n')) ++p;
        if(!*p)
            break;
        argv_50[n_50++]=p;
        while(*p && *p!=' ' && *p!='\t' && *p!='\n') ++p;
        if(*p)
        {
            *p='\0';
            ++p;
        }
    }
    return n_50;
}

//main(), starts the client, shows the prompt, runs commands in a loop
int main(int argc_50, char **argv_50)
{
    if(argc_50>=2)
        S1_HOST_50 = argv_50[1];
    if(argc_50>=3)
        S1_PORT_50 = atoi(argv_50[2]);

    /* Startup banner (no "Ctrl+D to quit.") */
    fprintf(stdout,"Connected target S1 at %s:%d\n", S1_HOST_50, S1_PORT_50);
    fprintf(stdout,"Enter commands (uploadf/downlf/removef/downltar/dispfnames). \n");

    char line_50[LINE_MAX_50];
    char *v_50[12];
    while(1)
    {
        fprintf(stdout,"s25client$ ");
        fflush(stdout);
        if(!fgets(line_50,sizeof line_50, stdin))
            break;
        int ac_50 = parse_line_50(line_50, v_50, (int)(sizeof v_50/sizeof v_50[0]));
        if(ac_50==0)
            continue;

        if(!strcmp(v_50[0],"uploadf"))
            cmd_uploadf_50(ac_50, v_50);
        else if(!strcmp(v_50[0],"downlf"))
            cmd_downlf_50(ac_50, v_50);
        else if(!strcmp(v_50[0],"removef"))
            cmd_removef_50(ac_50, v_50);
        else if(!strcmp(v_50[0],"downltar"))
            cmd_downltar_50(ac_50, v_50);
        else if(!strcmp(v_50[0],"dispfnames"))
            cmd_dispfnames_50(ac_50, v_50);
        else fprintf(stderr,"Unknown command only these are allowed (uploadf/downlf/removef/downltar/dispfnames). \n");
    }
    return 0;
}
