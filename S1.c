#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

//BACKLOG_10 defines the maximum no of waiting connections we allow
#define BACKLOG_10 16

//LINE_MAX_10 defines the max length of one text line we can send or receive
#define LINE_MAX_10 4096

//the size of the copy buffer
#define CHUNK_10   8192

// Ports for S1,S2,S3 and S4 where S1 listens and S2/S3/S4 are live
static const char *S1_LISTEN_HOST_10 = "0.0.0.0";
static int S1_LISTEN_PORT_10 = 5001;
static const char *S2_HOST_10 = "127.0.0.1";
static int S2_PORT_10 = 5002;
static const char *S3_HOST_10 = "127.0.0.1";
static int S3_PORT_10 = 5003;
static const char *S4_HOST_10 = "127.0.0.1";
static int S4_PORT_10 = 5004;

// sends exactly n_10 bytes to fd_10
static ssize_t write_fully_10(int fd_10, const void *buf_10, size_t n_10)
{
    const char *p_10 = (const char*)buf_10; size_t left_10 = n_10;
    while (left_10 > 0)
    {
        ssize_t w_10 = write(fd_10, p_10, left_10);
        if (w_10 < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        left_10 -= (size_t)w_10; p_10 += w_10;
    }
    return (ssize_t)n_10;
}
// this function reads n bytes from fd_10 and if the connection gets closed it stops
static ssize_t read_fully_10(int fd_10, void *buf_10, size_t n_10)
{
    char *p_10 = (char*)buf_10; size_t left_10 = n_10;
    while (left_10 > 0)
    {
        ssize_t r_10 = read(fd_10, p_10, left_10);
        if (r_10 == 0)
            return (ssize_t)(n_10 - left_10); /* EOF */
        if (r_10 < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        left_10 -= (size_t)r_10; p_10 += r_10;
    }
    return (ssize_t)n_10;
}
// reads text until a newline and stoes it into into buf_10
static int read_line_10(int fd_10, char *buf_10, size_t cap_10)
{
    size_t i_10 = 0;
    while (i_10 + 1 < cap_10)
    {
        char c_10; ssize_t r_10 = read(fd_10, &c_10, 1);
        if (r_10 == 0)
            break;
        if (r_10 < 0)
        {
            if (errno == EINTR) continue;
            return -1;
        }
        if (c_10 == '\n') break;
        buf_10[i_10++] = c_10;
    }
    buf_10[i_10] = '\0';
    return (int)i_10;
}

// Turn "~S1/.." from the argument into an absolute path under "/home/USER/S1/..."
// also creates the directory if it does not exists
static char *build_s1_path_10(const char *rel_10, int mk_10)
{
    const char *home_10 = getenv("HOME"); if (!home_10) home_10 = ".";
    char *base_10 = NULL; asprintf(&base_10, "%s/%s", home_10, "S1");
    if (mk_10)
        mkdir(base_10, 0700);

    char *out_10 = NULL;
    if (rel_10 && rel_10[0])
    {
        const char *r_10 = rel_10;
        if (strncmp(r_10, "~S1/", 4) == 0) r_10 += 4;  /* strip prefix */
        asprintf(&out_10, "%s/%s", base_10, r_10);
    }
    else
    {
        out_10 = strdup(base_10);
    }
    free(base_10);

    if (mk_10 && out_10)
    {
        // mkdir -p for parents
        char *tmp_10 = strdup(out_10);
        for (char *p_10 = tmp_10 + 1; *p_10; ++p_10)
        {
            if (*p_10 == '/')
            {
                *p_10 = '\0';
                mkdir(tmp_10, 0700);
                *p_10 = '/';
            }
        }
        struct stat st_10;
        if (stat(out_10, &st_10) != 0)
            mkdir(out_10, 0700);
        free(tmp_10);
    }
    return out_10;
}
//this functions gets the file extension from the arguments
static const char *ext_lower_10(const char *name_10)
{
    static char out_10[16];
    const char *dot_10 = strrchr(name_10, '.');
    if (!dot_10)
        return "";
    size_t n_10 = sizeof(out_10) - 1;
    strncpy(out_10, dot_10, n_10); out_10[n_10] = 0;
    for (char *p_10 = out_10; *p_10; ++p_10)
        *p_10 = (char)tolower((unsigned char)*p_10);
    return out_10;
}
// checks if "~S1/..." is present in path
static int path_is_s1_10(const char *p_10)
{
    return (p_10 && strncmp(p_10, "~S1/", 4) == 0);
}

//These are the Network helpers
//This function opens a TCP connection for host:port
static int connect_to_10(const char *host_10, int port_10)
{
    int fd_10 = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_10 < 0) return -1;
    struct sockaddr_in a_10; memset(&a_10, 0, sizeof a_10);
    a_10.sin_family = AF_INET; a_10.sin_port = htons((uint16_t)port_10);
    if (inet_pton(AF_INET, host_10, &a_10.sin_addr) != 1)
    {
        close(fd_10);
        return -1;
    }
    if (connect(fd_10, (struct sockaddr*)&a_10, sizeof a_10) != 0)
    {
        close(fd_10);
        return -1;
    }
    return fd_10;
}
// adds '\n' at the end of a line
static int send_line_10(int fd_10, const char *fmt_10, ...)
{
    char buf_10[LINE_MAX_10];
    va_list ap_10;
    va_start(ap_10, fmt_10);
    vsnprintf(buf_10, sizeof buf_10, fmt_10, ap_10);
    va_end(ap_10);
    size_t len_10 = strlen(buf_10);
    if (len_10 + 1 >= sizeof buf_10)
        return -1;
    buf_10[len_10++] = '\n';
    return (write_fully_10(fd_10, buf_10, len_10) == (ssize_t)len_10) ? 0 : -1;
}

// picks a backend port and chooses where to the send the files based on file extensions
static int pick_backend_port_10(const char *ext_10)
{
    if (strcmp(ext_10, ".pdf") == 0)
        return S2_PORT_10;
    if (strcmp(ext_10, ".txt") == 0)
        return S3_PORT_10;
    if (strcmp(ext_10, ".zip") == 0)
        return S4_PORT_10;

    return -1;
}
static const char *pick_backend_host_10(const char *ext_10)
{
    if (strcmp(ext_10, ".pdf") == 0)
        return S2_HOST_10;
    if (strcmp(ext_10, ".txt") == 0)
        return S3_HOST_10;
    if (strcmp(ext_10, ".zip") == 0)
        return S4_HOST_10;

    return NULL;
}

// These are the File helpers
// gets n bytes from a socket and writes to a file
static int recv_file_to_path_10(int fd_10, const char *dst_path_10, size_t size_10)
{
    int out_10 = open(dst_path_10, O_CREAT|O_TRUNC|O_WRONLY, 0600);
    if (out_10 < 0)
        return -1;
    char *buf_10 = (char*)malloc(CHUNK_10);
    if (!buf_10)
    {
        close(out_10);
        return -1;
    }
    size_t left_10 = size_10;
    while (left_10 > 0)
    {
        size_t want_10 = left_10 > CHUNK_10 ? CHUNK_10 : left_10;
        ssize_t r_10 = read_fully_10(fd_10, buf_10, want_10);
        if (r_10 <= 0)
        {
            free(buf_10);
            close(out_10);
            return -1;
        }
        if (write_fully_10(out_10, buf_10, (size_t)r_10) != r_10)
        {
            free(buf_10);
            close(out_10);
            return -1;
        }
        left_10 -= (size_t)r_10;
    }
    free(buf_10);
    close(out_10);
    return 0;
}
//sends the entire file to fd_10 (reads a file and push bytes to the socket)
static int send_file_from_path_10(int fd_10, const char *src_path_10, size_t *osz_10)
{
    int in_10 = open(src_path_10, O_RDONLY);
    if (in_10 < 0)
        return -1;
    struct stat st_10;
    if (fstat(in_10, &st_10) != 0)
    {
        close(in_10);
        return -1;
    }
    if (osz_10) *osz_10 = (size_t)st_10.st_size;

    char *buf_10 = (char*)malloc(CHUNK_10);
    if (!buf_10)
    {
        close(in_10);
        return -1;
    }
    for (;;)
    {
        ssize_t r_10 = read(in_10, buf_10, CHUNK_10);
        if (r_10 < 0)
        {
            if (errno==EINTR) continue;
            free(buf_10); close(in_10);
            return -1;
        }
        if (r_10 == 0) break;
        if (write_fully_10(fd_10, buf_10, (size_t)r_10) != r_10)
        {
            free(buf_10);
            close(in_10);
            return -1;
        }
    }
    free(buf_10);
    close(in_10);
    return 0;
}

// small helper for saving copies
//keeps files that user downloads like tar files etc
static int path_exists_10(const char *p_10)
{
    struct stat st_10; return (stat(p_10, &st_10) == 0);
}
static char *unique_dest_path_10(const char *dir_10, const char *name_10)
{
    char *dst_10 = NULL;
    asprintf(&dst_10, "%s/%s", dir_10, name_10);
    if(!path_exists_10(dst_10))
        return dst_10;

    const char *dot_10 = strrchr(name_10, '.');
    size_t base_len_10 = (dot_10 && dot_10 != name_10) ? (size_t)(dot_10 - name_10) : strlen(name_10);
    char *base_10 = strndup(name_10, base_len_10);
    const char *ext_10 = (dot_10 && dot_10 != name_10) ? dot_10 : "";

    free(dst_10); dst_10 = NULL;
    for(int n_10=1;;++n_10)
    {
        asprintf(&dst_10, "%s/%s_%d%s", dir_10, base_10, n_10, ext_10);
        if(!path_exists_10(dst_10))
        {
            free(base_10);
            return dst_10;
        }
        free(dst_10);
        dst_10 = NULL;
    }
}

// copies files to S1 directory
static int archive_copy_10(const char *subdir_10, const char *name_10, const char *src_path_10)
{
    char *rel_10 = NULL;
    asprintf(&rel_10, "~S1/%s", subdir_10);
    char *adir_10 = build_s1_path_10(rel_10, 1);  /* ensure dir exists */
    free(rel_10);

    char *dst_10 = unique_dest_path_10(adir_10, name_10);
    free(adir_10);
    if(!dst_10)
        return -1;

    int in_10 = open(src_path_10, O_RDONLY);
    if (in_10 < 0)
    {
        free(dst_10);
        return -1;
    }
    int out_10 = open(dst_10, O_CREAT|O_TRUNC|O_WRONLY, 0600);
    if (out_10 < 0)
    {
        close(in_10);
        free(dst_10);
        return -1;
    }

    char *buf_10 = (char*)malloc(CHUNK_10);
    if(!buf_10)
    {
        close(in_10);
        close(out_10);
        free(dst_10);
        return -1;
    }
    for(;;)
    {
        ssize_t r_10 = read(in_10, buf_10, CHUNK_10);
        if(r_10 < 0)
        {
            if(errno==EINTR) continue;
            free(buf_10); close(in_10);
            close(out_10);
            free(dst_10);
            return -1;
        }
        if(r_10 == 0) break;

        if(write_fully_10(out_10, buf_10, (size_t)r_10) != r_10)
        {
            free(buf_10); close(in_10); close(out_10); free(dst_10);
            return -1;
        }
    }
    free(buf_10); close(in_10); close(out_10); free(dst_10);
    return 0;
}

// Backend operations for S2/S3/S4
// send a non .c file to the relevant backend server
static int forward_store_10(const char *ext_10, const char *rel_dir_10, const char *fname_10, const char *tmp_path_10)
{
    int port_10 = pick_backend_port_10(ext_10);
    const char *host_10 = pick_backend_host_10(ext_10);
    if (port_10 < 0 || !host_10)
        return -1;

    //removes S1 from the path before sending it to backend
    const char *rel_only_10 = rel_dir_10;
    if (strncmp(rel_only_10, "~S1/", 4)==0)
        rel_only_10 += 4;

    //if the path was only S1, send "."
    const char *dir_field_10 = (rel_only_10 && *rel_only_10) ? rel_only_10 : "."; /* "." when "~S1/" */

    int fd_10 = connect_to_10(host_10, port_10);
    if (fd_10 < 0)
        return -1;

    //tells backend where to store the file
    if (send_line_10(fd_10, "STORE|%s|%s|", dir_field_10, fname_10) != 0)
    {
        close(fd_10);
        return -1;
    }

    // send the file size after the previous step
    struct stat st_10;
    if (stat(tmp_path_10, &st_10) != 0)
    {
        close(fd_10);
        return -1;
    }

    //then the size of the bytes
    if (send_line_10(fd_10, "%zu", (size_t)st_10.st_size) != 0)
    {
        close(fd_10);
        return -1;
    }

    if (send_file_from_path_10(fd_10, tmp_path_10, NULL) != 0)
    {
        close(fd_10);
        return -1;
    }

    char line_10[LINE_MAX_10];
    if (read_line_10(fd_10, line_10, sizeof line_10) <= 0)
    {
        close(fd_10); return -1;
    }
    close(fd_10);
    return (strncmp(line_10, "OK", 2)==0) ? 0 : -1;
}

//this function fetches files from the backend
static int backend_fetch_10(const char *ext_10, const char *rel_path_10, const char *tmp_path_10)
{
    int port_10 = pick_backend_port_10(ext_10);
    const char *host_10 = pick_backend_host_10(ext_10);

    if (port_10 < 0 || !host_10)
        return -1;

    //removes S1 from the user input
    const char *rel_only_10 = rel_path_10;
    if (strncmp(rel_only_10, "~S1/", 4)==0)
        rel_only_10 += 4;

    int fd_10 = connect_to_10(host_10, port_10);
    if (fd_10 < 0)
        return -1;
    if (send_line_10(fd_10, "FETCH|%s", rel_only_10) != 0)
    {
        close(fd_10); return -1;
    }

    char line_10[LINE_MAX_10];
    if (read_line_10(fd_10, line_10, sizeof line_10) <= 0)
    {
        close(fd_10); return -1;
    }
    if (strncmp(line_10, "OK|", 3) != 0)
    {
        close(fd_10);
        return -1;
    }

    char *tok_10=NULL, *save_10=NULL;
    tok_10 = strtok_r(line_10+3, "|", &save_10); /* name (unused) */
    tok_10 = strtok_r(NULL, "|", &save_10);      /* size */
    size_t size_10 = (size_t)strtoull(tok_10 ? tok_10 : "0", NULL, 10);

    int rc_10 = recv_file_to_path_10(fd_10, tmp_path_10, size_10);
    close(fd_10);
    return rc_10;
}

//this function deletes a file from the backend
static int backend_delete_10(const char *ext_10, const char *rel_path_10)
{
    int port_10 = pick_backend_port_10(ext_10);
    const char *host_10 = pick_backend_host_10(ext_10);
    if (port_10 < 0 || !host_10)
        return -1;
    const char *rel_only_10 = rel_path_10;
    if (strncmp(rel_only_10, "~S1/", 4)==0)
        rel_only_10 += 4;

    int fd_10 = connect_to_10(host_10, port_10);
    if (fd_10 < 0)
        return -1;
    if (send_line_10(fd_10, "DELETE|%s", rel_only_10) != 0)
    {
        close(fd_10); return -1;
    }
    char line_10[LINE_MAX_10];
    if (read_line_10(fd_10, line_10, sizeof line_10) <= 0)
    {
        close(fd_10); return -1;
    }
    close(fd_10);
    return (strncmp(line_10, "OK", 2)==0) ? 0 : -1;
}

//this function build the tar file for the required type (.c, .txt and .pdf)
static int backend_tar_10(const char *ext_10, char **out_tmp_path_10, size_t *out_size_10)
{
    int port_10 = pick_backend_port_10(ext_10);
    const char *host_10 = pick_backend_host_10(ext_10);
    if (port_10 < 0 || !host_10)
        return -1;

    int fd_10 = connect_to_10(host_10, port_10);
    if (fd_10 < 0)
        return -1;
    if (send_line_10(fd_10, "TAR|%s", ext_10) != 0)
    {
        close(fd_10); return -1;
    }

    char line_10[LINE_MAX_10];
    if (read_line_10(fd_10, line_10, sizeof line_10) <= 0)
    {
        close(fd_10); return -1;
    }
    if (strncmp(line_10, "OK|", 3) != 0)
    {
        close(fd_10); return -1;
    }

    char *tok_10, *save_10;
    tok_10 = strtok_r(line_10+3, "|", &save_10);
    tok_10 = strtok_r(NULL, "|", &save_10);
    size_t size_10 = (size_t)strtoull(tok_10 ? tok_10 : "0", NULL, 10);

    char *base_10 = build_s1_path_10("tmp", 1);
    char *full_10 = NULL; asprintf(&full_10, "%s/tar_%d.tmp", base_10, getpid());
    free(base_10);

    if (recv_file_to_path_10(fd_10, full_10, size_10) != 0)
    {
        free(full_10); close(fd_10);
        return -1;
    }
    close(fd_10);
    *out_tmp_path_10 = full_10;

    if (out_size_10) *out_size_10 = size_10;
    return 0;
}

//this functions lists all the files that the user uploaded onto the servers
static int backend_list_10(const char *ext_10, const char *rel_dir_10, char ***out_arr_10, int *out_cnt_10)
{
    int port_10 = pick_backend_port_10(ext_10);
    const char *host_10 = pick_backend_host_10(ext_10);
    if (port_10 < 0 || !host_10)
        return -1;

    const char *rel_only_10 = rel_dir_10;
    if (strncmp(rel_only_10, "~S1/", 4)==0)
        rel_only_10 += 4;

    int fd_10 = connect_to_10(host_10, port_10);
    if (fd_10 < 0)
        return -1;
    if (send_line_10(fd_10, "LIST|%s", (rel_only_10 && *rel_only_10) ? rel_only_10 : ".") != 0)
    {
        close(fd_10); return -1;
    }

    char line_10[LINE_MAX_10];
    if (read_line_10(fd_10, line_10, sizeof line_10) <= 0)
    {
        close(fd_10); return -1;
    }
    if (strcmp(line_10, "OK") != 0)
    {
        close(fd_10); return -1;
    }

    int cap_10 = 8, cnt_10 = 0;
    char **arr_10 = (char**)malloc(sizeof(char*)*cap_10);
    if (!arr_10)
    {
        close(fd_10); return -1;
    }

    for (;;)
    {
        int n_10 = read_line_10(fd_10, line_10, sizeof line_10);
        if (n_10 <= 0)
            break;
        if (strcmp(line_10, "END")==0)
            break;
        if (strncmp(line_10, "NAME|", 5)==0)
        {
            const char *nm_10 = line_10 + 5;
            if (cnt_10 == cap_10)
            {
                cap_10 *= 2;
                arr_10 = (char**)realloc(arr_10, sizeof(char*)*cap_10);
            }
            arr_10[cnt_10++] = strdup(nm_10);
        }
    }
    close(fd_10);
    *out_arr_10 = arr_10; *out_cnt_10 = cnt_10;
    return 0;
}

// handlers for all the 5 commands (uploadf, downlf, removef, downltar, dispfnames)

//this is the uploadf handler
//handles the user uploads and send .c files to S1 directory
// also checks if the max files does not exceed 3 for this command
static void handle_uploadf_10(int cfd_10, char *line_10)
{
    char *save_10 = NULL;
    strtok_r(line_10, "|", &save_10);          /* "UPLOADF" */
    char *nstr_10  = strtok_r(NULL, "|", &save_10);
    char *dest_10  = strtok_r(NULL, "|", &save_10);
    int n_10 = nstr_10 ? atoi(nstr_10) : 0;

    if (n_10 < 1 || n_10 > 3 || !dest_10 || !path_is_s1_10(dest_10))
    {
        send_line_10(cfd_10, "ERR|bad upload header");
        return;
    }

    char *tmpdir_10 = build_s1_path_10("tmp", 1);

    for (int i_10 = 0; i_10 < n_10; ++i_10)
    {
        char meta_10[LINE_MAX_10];
        if (read_line_10(cfd_10, meta_10, sizeof meta_10) <= 0)
        {
            free(tmpdir_10); return;
        }

        if (strncmp(meta_10, "FILEMETA|", 9)!=0)
        {
            free(tmpdir_10);
            return;
        }

        char *sav2_10 = NULL;
        char *fname_10 = strtok_r(meta_10+9, "|", &sav2_10);
        char *szstr_10 = strtok_r(NULL, "|", &sav2_10);
        size_t fsz_10 = (size_t)strtoull(szstr_10?szstr_10:"0", NULL, 10);

        char *tmpfile_10 = NULL; asprintf(&tmpfile_10, "%s/%s", tmpdir_10, fname_10?fname_10:"file");

        if (recv_file_to_path_10(cfd_10, tmpfile_10, fsz_10) != 0)
        {
            free(tmpfile_10); free(tmpdir_10);
            return;
        }

        const char *ext_10 = ext_lower_10(fname_10?fname_10:"");

        if (strcmp(ext_10, ".c")==0)
        {
            char *dst_dir_10  = build_s1_path_10(dest_10, 1);
            char *dst_path_10 = NULL; asprintf(&dst_path_10, "%s/%s", dst_dir_10, fname_10);
            rename(tmpfile_10, dst_path_10);
            free(dst_path_10); free(dst_dir_10);
        }
        else if
        (!strcmp(ext_10, ".pdf") || !strcmp(ext_10, ".txt") || !strcmp(ext_10, ".zip"))
        {
            if (forward_store_10(ext_10, dest_10, fname_10, tmpfile_10) == 0)
            {
                unlink(tmpfile_10);  // removes local temp after successful forward
            }
        }
        free(tmpfile_10);
    }
    free(tmpdir_10);
    send_line_10(cfd_10, "OK");
}

//this is the downlf handler
//downloads the required files for the client under ~/S1/downloaded_files/
static void handle_downlf_10(int cfd_10, char *line_10)
{
    strtok_r(line_10, "|", &line_10); /* "DOWNLF" */
    char *nstr_10 = strtok_r(NULL, "|", &line_10);
    int n_10 = nstr_10 ? atoi(nstr_10) : 0;

    if (n_10 < 1 || n_10 > 2)
    {
        send_line_10(cfd_10, "ERR|bad downlf header");
        return;
    }

    for (int i_10 = 0; i_10 < n_10; ++i_10)
    {
        char *pp_10 = strtok_r(NULL, "|", &line_10);
        if (!pp_10 || !path_is_s1_10(pp_10))
        {
            send_line_10(cfd_10, "FILENOTFOUND|%s", pp_10?pp_10:"");
            continue;
        }

        const char *ext_10 = ext_lower_10(pp_10);

        if (!strcmp(ext_10, ".c"))
        {
            char *full_10 = build_s1_path_10(pp_10, 0);
            struct stat st_10;
            if (stat(full_10, &st_10) != 0 || !S_ISREG(st_10.st_mode))
            {
                send_line_10(cfd_10, "FILENOTFOUND|%s", pp_10);
                free(full_10);
                continue;
            }
            size_t sz_10 = (size_t)st_10.st_size;
            const char *basename_10 = strrchr(full_10, '/');
            basename_10 = basename_10 ? basename_10+1 : full_10;

            archive_copy_10("downloaded_files", basename_10, full_10);

            send_line_10(cfd_10, "FILERESP|%s|%zu", basename_10, sz_10);
            send_file_from_path_10(cfd_10, full_10, NULL);
            free(full_10);

        }
        else if (!strcmp(ext_10, ".pdf") || !strcmp(ext_10, ".txt") || !strcmp(ext_10, ".zip"))
        {
            // fetch into a temp file from the backend, then send & archive
            char *tmpdir_10 = build_s1_path_10("tmp", 1);
            char *tmpout_10 = NULL; asprintf(&tmpout_10, "%s/fetch_%d.tmp", tmpdir_10, getpid());
            free(tmpdir_10);

            if (backend_fetch_10(ext_10, pp_10, tmpout_10) != 0)
            {
                send_line_10(cfd_10, "FILENOTFOUND|%s", pp_10);
                free(tmpout_10);
                continue;
            }
            struct stat st_10; stat(tmpout_10, &st_10);
            size_t size_10 = (size_t)st_10.st_size;
            const char *base_10 = strrchr(pp_10, '/');
            base_10 = base_10? base_10+1 : pp_10;

            archive_copy_10("downloaded_files", base_10, tmpout_10);

            send_line_10(cfd_10, "FILERESP|%s|%zu", base_10, size_10);
            send_file_from_path_10(cfd_10, tmpout_10, NULL);
            unlink(tmpout_10); free(tmpout_10);

        }
        else
        {
            send_line_10(cfd_10, "FILENOTFOUND|%s", pp_10);
        }
    }
    send_line_10(cfd_10, "DONE");
}

// this is the handler for removef
//deletes the .c locally or .pdf/.txt/.zip files from backend
static void handle_removef_10(int cfd_10, char *line_10)
{
    strtok_r(line_10, "|", &line_10);
    char *nstr_10 = strtok_r(NULL, "|", &line_10);
    int n_10 = nstr_10 ? atoi(nstr_10) : 0;
    if (n_10 < 1 || n_10 > 2)
    {
        send_line_10(cfd_10, "ERR|bad removef header");
        return;
    }

    for (int i_10 = 0; i_10 < n_10; ++i_10)
    {
        char *pp_10 = strtok_r(NULL, "|", &line_10);
        if (!pp_10 || !path_is_s1_10(pp_10))
        {
            send_line_10(cfd_10, "REMERR|%s|bad_path", pp_10?pp_10:"");
            continue;
        }
        const char *ext_10 = ext_lower_10(pp_10);

        if (!strcmp(ext_10, ".c"))
        {
            char *full_10 = build_s1_path_10(pp_10, 0);
            if (unlink(full_10) == 0)
                send_line_10(cfd_10, "REMOK|%s", pp_10);
            else
                send_line_10(cfd_10, "REMERR|%s|%s", pp_10, strerror(errno));
            free(full_10);

        }
        else if (!strcmp(ext_10, ".pdf") || !strcmp(ext_10, ".txt") || !strcmp(ext_10, ".zip"))
        {
            if (backend_delete_10(ext_10, pp_10)==0)
                send_line_10(cfd_10, "REMOK|%s", pp_10);
            else
                send_line_10(cfd_10, "REMERR|%s|NOT FOUND", pp_10);
        }
        else
        {
            send_line_10(cfd_10, "REMERR|%s|unsupported", pp_10);
        }
    }
}

// this is the handler for downltar
// send the required tar files to ~/S1/tar_files/ with fixed names:
/*          .c   -> cfiles.tar
          .pdf -> pdfs.tar
          .txt -> textiles.tar
*/
static void handle_downtar_10(int cfd_10, char *line_10)
{
    strtok_r(line_10, "|", &line_10);
    char *type_10 = strtok_r(NULL, "|", &line_10);
    if (!type_10)
    {
        send_line_10(cfd_10, "ERR|missing_type");
        return;
    }

    if (!strcmp(type_10, ".c"))
    {
        // Build tar of all .c under ~/S1
        char *root_10   = build_s1_path_10("", 1);
        char *tmpdir_10 = build_s1_path_10("tmp", 1);
        char *tar_10    = NULL; asprintf(&tar_10, "%s/cfiles.tar", tmpdir_10);
        free(tmpdir_10);

        char *cmd_10 = NULL;
        asprintf(&cmd_10, "cd '%s' && tar -cf '%s' $(find . -type f -name '*.c' | sed 's|^\\./||') 2>/dev/null", root_10, tar_10);
        int rc_10 = system(cmd_10);
        (void)rc_10;
        free(cmd_10);
        free(root_10);

        struct stat st_10;
        if (stat(tar_10, &st_10)!=0)
        {
            send_line_10(cfd_10, "ERR|no_c_files");
            free(tar_10); return;
        }
        size_t sz_10 = (size_t)st_10.st_size;

        //copy with fixed name cfiles.tar
        archive_copy_10("tar_files", "cfiles.tar", tar_10);

        send_line_10(cfd_10, "FILERESP|cfiles.tar|%zu", sz_10);
        send_file_from_path_10(cfd_10, tar_10, NULL);
        unlink(tar_10); free(tar_10);

    }
    else if (!strcmp(type_10, ".pdf") || !strcmp(type_10, ".txt"))
    {
        char *tmp_10 = NULL; size_t sz_10 = 0;
        if (backend_tar_10(type_10, &tmp_10, &sz_10) != 0)
        {
            send_line_10(cfd_10, "ERR|tar_backend");
            return;
        }

        const char *fname_10 = (!strcmp(type_10,".pdf")) ? "pdfs.tar" : "textiles.tar";

        archive_copy_10("tar_files", fname_10, tmp_10);

        send_line_10(cfd_10, "FILERESP|%s|%zu", fname_10, sz_10);
        send_file_from_path_10(cfd_10, tmp_10, NULL);
        unlink(tmp_10); free(tmp_10);
    }
    else
    {
        send_line_10(cfd_10, "ERR|unsupported_type");
    }
}

//this is the handler for dispfnames
// lists all the files uploaded by the user
static int compare_str_10(const void *a_10, const void *b_10)
{
    char * const *aa_10 = (char* const*)a_10;
    char * const *bb_10 = (char* const*)b_10;
    return strcasecmp(*aa_10, *bb_10);
}
static void handle_disp_10(int cfd_10, char *line_10)
{
    strtok_r(line_10, "|", &line_10);
    char *pp_10 = strtok_r(NULL, "|", &line_10);
    if (!pp_10 || !path_is_s1_10(pp_10))
    {
        send_line_10(cfd_10, "ERR|bad_path");
        return;
    }

    //lists all local .c files in that folder
    char *dir_10 = build_s1_path_10(pp_10, 0);
    DIR *d_10 = opendir(dir_10);
    char **cvec_10 = NULL; int ccount_10 = 0, ccap_10 = 8;
    cvec_10 = (char**)malloc(sizeof(char*)*ccap_10);

    if (d_10)
    {
        struct dirent *e_10;
        while ((e_10 = readdir(d_10)))
        {
            if (e_10->d_type == DT_REG)
            {
                const char *ext_10 = ext_lower_10(e_10->d_name);
                if (!strcmp(ext_10, ".c"))
                {
                    if (ccount_10==ccap_10)
                    {
                        ccap_10*=2; cvec_10=(char**)realloc(cvec_10,sizeof(char*)*ccap_10);
                    }
                    cvec_10[ccount_10++] = strdup(e_10->d_name);
                }
            }
        }
        closedir(d_10);
    }

    // asks S2/S3/S4 for the files
    char **pvec_10=NULL, **tvec_10=NULL, **zvec_10=NULL;
    int pcount_10=0, tcount_10=0, zcount_10=0;
    backend_list_10(".pdf", pp_10, &pvec_10, &pcount_10);
    backend_list_10(".txt", pp_10, &tvec_10, &tcount_10);
    backend_list_10(".zip", pp_10, &zvec_10, &zcount_10);

    if (ccount_10>1)
        qsort(cvec_10, ccount_10, sizeof(char*), compare_str_10);
    if (pcount_10>1)
        qsort(pvec_10, pcount_10, sizeof(char*), compare_str_10);
    if (tcount_10>1)
        qsort(tvec_10, tcount_10, sizeof(char*), compare_str_10);
    if (zcount_10>1)
        qsort(zvec_10, zcount_10, sizeof(char*), compare_str_10);

    //lists all the files to the client
    send_line_10(cfd_10, "LISTBEGIN");
    for (int i_10=0;i_10<ccount_10;++i_10)
    { 
        send_line_10(cfd_10, "NAME|.c|%s",   cvec_10[i_10]); 
    }
    for (int i_10=0;i_10<pcount_10;++i_10)
    { 
        send_line_10(cfd_10, "NAME|.pdf|%s", pvec_10[i_10]); 
    }
    for (int i_10=0;i_10<tcount_10;++i_10)
    { 
        send_line_10(cfd_10, "NAME|.txt|%s", tvec_10[i_10]); 
    }
    for (int i_10=0;i_10<zcount_10;++i_10)
    { 
        send_line_10(cfd_10, "NAME|.zip|%s", zvec_10[i_10]); 
    }
    send_line_10(cfd_10, "LISTEND");

    // frees memory
    for (int i_10=0;i_10<ccount_10;++i_10) 
        free(cvec_10[i_10]); 
    free(cvec_10);
    for (int i_10=0;i_10<pcount_10;++i_10) 
        free(pvec_10[i_10]); 
    free(pvec_10);
    for (int i_10=0;i_10<tcount_10;++i_10) 
        free(tvec_10[i_10]); 
    free(tvec_10);
    for (int i_10=0;i_10<zcount_10;++i_10) 
        free(zvec_10[i_10]); 
    free(zvec_10);
    free(dir_10);
}

//prcclient(): one child per client connection
// this function waits for command from the client, calls the matching handler and repeats the process until the client disconnects
static void prcclient_10(int cfd_10)
{
    for (;;)
    {
        char line_10[LINE_MAX_10];
        int n_10 = read_line_10(cfd_10, line_10, sizeof line_10);
        if (n_10 <= 0)
            break; /* client closed */

        if (!strncmp(line_10, "UPLOADF|", 8))
            handle_uploadf_10(cfd_10, line_10);
        else if (!strncmp(line_10, "DOWNLF|", 7))
            handle_downlf_10(cfd_10, line_10);
        else if (!strncmp(line_10, "REMOVEF|", 8))
            handle_removef_10(cfd_10, line_10);
        else if (!strncmp(line_10, "DOWNTAR|", 8))
            handle_downtar_10(cfd_10, line_10);
        else if (!strncmp(line_10, "DISP|", 5))
            handle_disp_10(cfd_10, line_10);
        else
            send_line_10(cfd_10, "ERR|unknown_cmd");
    }
}

//main(), starts server and accepts clients

//cleans up finished child processes so they dont become zombies
static void reap_10(int s_10)
{
    (void)s_10;
    while (waitpid(-1, NULL, WNOHANG)>0){}
}

int main(int argc, char **argv)
{
    // allow overriding ports via argv
    if (argc >= 2)
        S1_LISTEN_PORT_10 = atoi(argv[1]);
    if (argc >= 4)
    {
        S2_HOST_10 = argv[2];
        S2_PORT_10 = atoi(argv[3]);
    }
    if (argc >= 6)
    {
        S3_HOST_10 = argv[4];
        S3_PORT_10 = atoi(argv[5]);
    }
    if (argc >= 8)
    {
        S4_HOST_10 = argv[6];
        S4_PORT_10 = atoi(argv[7]);
    }

    // ensure ~/S1 exists
    char *root_10 = build_s1_path_10("", 1); free(root_10);

    signal(SIGCHLD, reap_10);

    //creates create, bind and listen on a TCP socket
    int lfd_10 = socket(AF_INET, SOCK_STREAM, 0);
    int opt_10 = 1;

    setsockopt(lfd_10, SOL_SOCKET, SO_REUSEADDR, &opt_10, sizeof opt_10);
    struct sockaddr_in addr_10; memset(&addr_10, 0, sizeof addr_10);

    addr_10.sin_family = AF_INET;
    addr_10.sin_port = htons((uint16_t)S1_LISTEN_PORT_10);

    inet_pton(AF_INET, S1_LISTEN_HOST_10, &addr_10.sin_addr);

    if (bind(lfd_10, (struct sockaddr*)&addr_10, sizeof addr_10) != 0)
    {
        perror("bind");
        exit(1);
    }
    if (listen(lfd_10, BACKLOG_10) != 0)
    {
        perror("listen");
        exit(1);
    }

    fprintf(stderr, "[S1] listening on %s:%d\n", S1_LISTEN_HOST_10, S1_LISTEN_PORT_10);

    for (;;)
    {
        struct sockaddr_in cli_10;
        socklen_t cl_10 = sizeof cli_10;
        int cfd_10 = accept(lfd_10, (struct sockaddr*)&cli_10, &cl_10);
        if (cfd_10 < 0)
        {
            if (errno==EINTR)
                continue;
            perror("accept");
            continue;
        }

        pid_t pid_10 = fork();
        if (pid_10 == 0)
        {
            close(lfd_10);
            prcclient_10(cfd_10);
            close(cfd_10);
            _exit(0);
        }
        else if (pid_10 > 0)
        {
            close(cfd_10);
        }
        else
        {
            perror("fork");
            close(cfd_10);
        }
    }
    return 0;
}
