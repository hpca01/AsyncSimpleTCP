#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define PORT_NUM 8080
#define BUFF_SIZE 20000
#define BACKLOG 100
#define HTML_FILE "text/html"
#define IMG_JPEG_FILE "image/jpeg"
#define PDF_FILE "application/pdf"

extern const char *FMT;

typedef enum
{
    GET,
    POST,
    PUT
} ReqType;

/// Route has request type, host address, requested resource(route) and userstring
typedef struct
{
    ReqType type;
    char *route;
    char *host;
    char *user_string;
} Route;

typedef struct
{
    int *socket;
    char *host_name;
} thread_args;

typedef struct
{
    unsigned char *buffer;
    struct stat fp;
    char *filetype;
} FileOut;

void *get_in_addr(struct sockaddr *sa);
void check(int value, char *err_str);
void *handle_conn_wrapper(void *arg);
void handle_new_conn(int *accepted_socket, void *additional_args);
Route *parse_http(char *input);
Route *parse_route(char *input, Route *req);
char *humanize(Route *route, pid_t *pid);
void pretty_print_route(Route *route);
char *translate_reqtype(ReqType type);
int read_file(Route *route, FileOut *out);
void write_file(FileOut *out, int sockfd);
char *parse_file_type(char *input);
void free_fout(FileOut *out);