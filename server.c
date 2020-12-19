#include "server.h"
#include <pthread.h>

const char *FMT = "HTTP/1.1 200 OK\r\n"
                  "Content-length: %ld\r\n"
                  "Content-Type: %s\r\n"
                  "\r\n"
                  "%s";

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//easy way to handle fails.
void check(int value, char *err_str)
{
    if (value < 0)
    {
        perror(err_str);
        exit(EXIT_FAILURE);
    }
}

/// Deprecated
char *humanize(Route *route, pid_t *pid)
{
    char *req_type = translate_reqtype(route->type);

    int len_str = snprintf(NULL, 0, "<html>\n<head>\n<title>Hello Process %d !</title>\n</head>\n<body>\n<h1>%s Request</h1>\n<p> Host: %s \n<p> User-Agent: %s \n<p> Looking for: <b>%s</b> \n<p>Answered by PID: %d \n</body>\n</html>\n",
                           *pid, req_type, route->host, route->user_string, route->route, *pid); // write a variadic function for this.

    char *buf = malloc(len_str + 1); //TODO Free
    snprintf(buf, len_str + 1, "<html>\n<head>\n<title>Hello Process %d !</title>\n</head>\n<body>\n<h1>%s Request</h1>\n<p> Host: %s \n<p> User-Agent: %s \n<p> Looking for: <b>%s</b> \n<p>Answered by PID: %d \n</body>\n</html>\n",
             *pid, req_type, route->host, route->user_string, route->route, *pid);

    int output = snprintf(NULL, 0, FMT, strlen(buf), "text/html", buf);

    char *out = malloc(output + 1);

    snprintf(out, output + 1, FMT, strlen(buf), "text/html", buf);

    free(buf);
    return out;
}

void handle_new_conn(int *p_accepted_socket, void *additional_args)
{
    int accepted_socket = *p_accepted_socket;
    free(p_accepted_socket);
    char *from = (char *)additional_args;
    printf(">> Incoming message from %s : \n", from);

    //pid_t child_pid = getpid(); // underlying size is an int, so #10 chars max.

    char message[BUFF_SIZE] = {0};

    long value = read(accepted_socket, message, BUFF_SIZE); //don't use pread socket is not "seekable"

    if (value < 0)
    {
        close(accepted_socket);
        perror("Error reading incoming msg");
        exit(EXIT_FAILURE);
    }

    Route *route = parse_http(message);

    FileOut *file_out = calloc(1, sizeof(FileOut));

    int success = read_file(route, file_out);

    switch (success)
    {
    case -1: //error locating file
        break;
    case -2: //file extension not supported
        break;
    case 1: //works!
        write_file(file_out, accepted_socket);
        free_fout(file_out);
        break;
    }

    // char *output = humanize(route, &child_pid);

    // int write_result = write(accepted_socket, output, strlen(output));
    // if (write_result < 0)
    // {
    //     close(accepted_socket);
    //     perror("Error writing outgoing msg");
    //     exit(EXIT_FAILURE);
    // }
    close(accepted_socket);

    //free(output);
    free(route);
}

Route *parse_route(char *token, Route *req)
{
    char *request;
    int i = 0;
    while ((request = strsep(&token, " ")) != NULL)
    {
        if (i == 0)
        {
            // req type
            if (strcmp(request, "GET") == 0)
            {
                req->type = GET;
            }
            else if (strcmp(request, "POST") == 0)
                req->type = POST;
            else if (strcmp(request, "PUT") == 0)
                req->type = PUT;
        }
        else if (i == 1)
        {
            req->route = request;
        }
        i++;
    }
    return req;
}

char *translate_reqtype(ReqType type)
{
    switch (type)
    {
    case GET:
        return "GET";
    case POST:
        return "POST";
    case PUT:
        return "PUT";
    }
    return "OTHER";
}

void pretty_print_route(Route *route)
{
    char *type = translate_reqtype(route->type);
    printf("Route :\n\t%s\n\t%s\n\t%s\n", type, route->route, route->host);
    return;
}

Route *parse_http(char *input)
{
    char *cp = strdup(input);

    char *token;

    //first line is request type, resource uri and http protocol
    Route *route = malloc(sizeof(Route));

    int i = 0;
    while ((token = strtok_r(cp, "\n", &cp)))
    {
        if (i == 0)
        {
            route = parse_route(token, route);
        }
        else if (i == 1)
        {
            char *space = strchr(token, 32); //32 is space character
            if (space != NULL)
                route->host = space + 1; // add one to get the next char over don't want to get the extra space
        }
        else if (i == 2)
            route->user_string = strchr(token, 32) + 1;
        i += 1;
    }
    return route;
}

int main()
{

    struct addrinfo hints, *res;
    struct sockaddr_storage incoming_addr; // this is to make it ipv4 or ipv6 agnostic
    int server_file_d;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //only ipv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    check(getaddrinfo(NULL, "8080", &hints, &res), "Error with instantiating addrinfo struct");

    // Create Socket, bind to it
    // If Protocol is 0 it's automatically selected
    check(server_file_d = socket(res->ai_family, res->ai_socktype, res->ai_protocol), "Cannot create the socket"); //change to PF_INET for the sake of "correctedness"

    //setsockopt to be able to reuse to an unclosed socket...usually happens if you close and re-start the server
    int yes = 1;

    check(setsockopt(server_file_d, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)), "Could not reuse the unclosed socket.");

    /*
        binding to socket requires a socketaddr struct,
        struct is a generic, the actual definition is determined byt the type of network
        IP networking:
        https://pubs.opengroup.org/onlinepubs/009695399/basedefs/netinet/in.h.html

        struct sockaddr_in
        {
            __uint8_t sin_len; //note this is specific to linux...not portable
            sa_family_t sin_family;
            in_port_t sin_port;
            struct in_addr sin_addr;
            char sin_zero[8]; //technically removed...see link
        };

        NOTE: you do not have to pack it by hand,  use the helper func "getaddrinfo" and use the result ptr.
    */

    check(bind(server_file_d, res->ai_addr, res->ai_addrlen), "Failed to bind socket to address/port");

    //Now listen to the bound port
    check(listen(server_file_d, BACKLOG), "Failed to listen");
    //listen doesn't mean accept

    socklen_t addrlen = sizeof(incoming_addr);

    while (1)
    {
        //todo replace code w/ ASYNC version
        printf("\n~~~~~~~~~Waiting to accept a new conn~~~~~~~~\n\n\n");
    }
    close(server_file_d);
    return 0;
}
void *handle_conn_wrapper(void *arg)
{
    thread_args *args = (thread_args *)arg;
    handle_new_conn(args->socket, args->host_name);
    free(args);
    pthread_exit(NULL);
}

/// read file function takes p_Route and p_FileOut
///
/// return -1 if file not found
/// return -2 if file not supported
/// return 1 if found and supported
int read_file(Route *route, FileOut *file_out)
{
    //route requested resource = file name inside of files dir
    struct stat sbuf;
    char files[] = "./files";
    char *fp = realpath(files, NULL);
    char *c_fp = malloc(strlen(fp) + strlen(route->route) + 1); //TODO FREE
    sprintf(c_fp, "%s%s", fp, route->route);
    printf("\n\nFile path %s\n", c_fp);

    if (stat(c_fp, &sbuf) < 0)
    {
        perror("Error locating file");
        free(c_fp);
        return -1;
    }

    unsigned char *buffer = calloc(sbuf.st_size, sizeof(unsigned char)); //TODO Free

    FILE *f_to_read = fopen(c_fp, "rb");
    fread(buffer, sizeof(unsigned char), sbuf.st_size, f_to_read);
    fflush(stdout);

    printf("\nSize of file : %ld\n", sbuf.st_size);

    printf("\n%s\n", buffer);

    file_out->buffer = buffer;
    file_out->fp = sbuf;
    char *ftype = parse_file_type(c_fp);
    if (ftype == NULL)
    {
        perror("Error file not supported");
        free(c_fp);
        free(buffer);
        fclose(f_to_read);
        return -2;
    }
    file_out->filetype = ftype;

    fflush(stdout);
    free(c_fp);
    fclose(f_to_read);
    return 1;
}

char *parse_file_type(char *input)
{
    if (strstr(input, ".html") != NULL)
    {
        char *out = calloc(sizeof(char), strlen(HTML_FILE));
        strcpy(out, HTML_FILE);
        return out;
    }
    if (strstr(input, ".pdf") != NULL)
    {
        char *out = calloc(sizeof(char), strlen(PDF_FILE));
        strcpy(out, PDF_FILE);
        return out;
    }
    if (strstr(input, ".jpg") != NULL)
    {
        char *out = calloc(sizeof(char), strlen(IMG_JPEG_FILE));
        strcpy(out, IMG_JPEG_FILE);
        return out;
    }
    if (strstr(input, ".jpeg") != NULL)
    {
        char *out = calloc(sizeof(char), strlen(IMG_JPEG_FILE));
        strcpy(out, IMG_JPEG_FILE);
        return out;
    }
    return NULL;
}

void free_fout(FileOut *out)
{
    free(out->filetype);
    free(out->buffer);
    free(out);
}

void write_file(FileOut *out, int accepted_socket)
{
    //saw an implementation w/ mmap being used...thinking of using that
    FILE *sockfd = fdopen(accepted_socket, "r+");
    fprintf(sockfd, "HTTP/1.1 200 OK\n");
    fprintf(sockfd, "Server: My Simple TCP Server\n");
    fprintf(sockfd, "Content-length: %d\n", (int)out->fp.st_size);
    fprintf(sockfd, "Content-type: %s\n", out->filetype);
    fprintf(sockfd, "\r\n");

    fwrite(out->buffer, sizeof(unsigned char), out->fp.st_size, sockfd); //should check to see if all of the content is written, there is no guarantee that fwrite will write ALL of the data out.
    fflush(sockfd);
    fclose(sockfd);
}