/*
A collection of functions to handle requests
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define MAXMESSAGES 16

struct httpRequest
{
    int requestVersion; // 0 if 1.0, 1 if 1.1
    char *message;      // first line of message
    char *fileName;     // middle name of file
    int returnCode;
    char *headers;
    char *messageBody;
    char *contentType;
    int contentLength;
};

struct thread_args
{
    int connfd;
    const char *document_root;
};

void parseHttpRequest(struct httpRequest *request, char *messageLines[], int numMessages);
void findFile(struct httpRequest *request, const char *document_root);
void generate_headers(struct httpRequest *request);

void *run_http_thread(void *vargp)
{
    char message[BUFSIZE];
    int num_read;                 /* num bytes read */
    int num_sent;                 /* num bytes sent */
    struct thread_args *args = vargp;
    int connfd = args->connfd;
    const char *document_root = args->document_root;

    char *messageLines[MAXMESSAGES];
    int numMessages = 0;

    /* detach this thread from parent thread */
    if (pthread_detach(pthread_self()) != 0)
    {
        printf("error detaching\n");
        exit(1);
    }

    free(args);

    /* recv: read input string from the client */
    int twoNewLine = 1;
    while (twoNewLine)
    {
        // read message
        bzero(message, BUFSIZE);
        num_read = recv(connfd, message, BUFSIZE, 0);
        if (num_read < 0)
        {
            // TODO close with error
            exit(1);
        }

        // check for empty line (telnet connection), ends early to not save empty new line
        if (strcmp(message, "\n") == 0 || strcmp(message, "\r\n") == 0)
        {
            twoNewLine = 0;
            continue;
        }

        // check for end of message browser connection, splits message to lines and saves to messageLines
        int len = num_read;
        if (len >= 4 && message[len - 4] == '\r' && message[len - 3] == '\n' && message[len - 2] == '\r' && message[len - 1] == '\n')
        {
            char *line = strtok(message, "\r\n");

            while (line != NULL)
            {
                if (numMessages >= MAXMESSAGES)
                {
                    exit(1);
                }

                messageLines[numMessages] = strdup(line);
                if (messageLines[numMessages] == NULL)
                {
                    // TODO close with error
                }

                numMessages++;

                line = strtok(NULL, "\r\n");
            }

            twoNewLine = 0;
            continue;
        }

        // clean newline for telnet messages
        if (len >= 2 && message[len - 2] == '\r' && message[len - 1] == '\n')
        {
            message[len - 2] = '\0';
        }
        else if (len >= 1 && message[len - 1] == '\n')
        {
            message[len - 1] = '\0';
        }

        messageLines[numMessages] = strdup(message);
        numMessages += 1;
        if (numMessages >= MAXMESSAGES)
        {
            // TODO close with error
        }
        printf("server received %d bytes: %s\n", num_read, messageLines[numMessages - 1]);
    }

    printf("all lines \n");
    for (int i = 0; i < numMessages; i++)
    {
        printf("%s %i \n", messageLines[i], i);
    }

    // create empty http request
    struct httpRequest *request = calloc(1, sizeof(struct httpRequest));
    parseHttpRequest(request, messageLines, numMessages);

    //if valid request at this point, look for file
    if (request->returnCode == 200) {findFile(request, document_root);}


    //generate headers and send back
    generate_headers(request);
    send(connfd, request->headers, strlen(request->headers), 0);
    send(connfd, request->messageBody, request->contentLength, 0);


    //close connection and thread
    shutdown(connfd, 0);
    close(connfd);
    return NULL;
}

void parseHttpRequest(struct httpRequest *request, char *messageLines[], int numMessages)
{
    // have you given me anything
    if (numMessages < 1)
    {
        request->returnCode = 400;
        return;
    }

    // is it in the form "GET <file> <version>"
    request->message = messageLines[0];
    printf(request->message);

    // seperate message into array by spaces
    char *dupForTokens = strdup(request->message);
    char *seperated[3];
    int arrSize = 0;
    char *token = strtok(dupForTokens, " ");

    while (token != NULL)
    {
        seperated[arrSize++] = token;
        token = strtok(NULL, " ");

        // too many arguments
        if (arrSize > 3)
        {
            request->returnCode = 400;
            return;
        }
    }

    // too few arguments
    if (arrSize != 3)
    {
        request->returnCode = 400;
        return;
    }

    // not GET request
    if (!(strcmp(seperated[0], "GET") == 0))
    {
        request->returnCode = 400;
        return;
    }

    // set request version, error out otherwise
    if (strcmp(seperated[2], "HTTP/1.1") == 0)
    {
        request->requestVersion = 1;
    }
    else if (strcmp(seperated[2], "HTTP/1.0") == 0)
    {
        request->requestVersion = 0;
    }
    else
    {
        request->returnCode = 400;
        return;
    }

    // is there a host?
    if (request->requestVersion == 1)
    {
        int hostFound = 0;
        for (int i = 1; i < numMessages; i++)
        {
            if (strncasecmp(messageLines[i], "Host:", 5) == 0)
            {
                hostFound = 1;
                break;
            }
        }

        if (!hostFound)
        {
            request->returnCode = 400;
            return;
        }
    }

    // assume file is correct for now
    request->fileName = seperated[1];
    // prelim set so i can print shit out, not gaurenteeded to be correct
    request->returnCode = 200;
}


void findFile(struct httpRequest *request, const char *document_root)
{
    enum file_type
    {
        FILE_TYPE_UNSUPPORTED,
        FILE_TYPE_HTML,
        FILE_TYPE_JPG,
        FILE_TYPE_GIF
    };

    char *root_path = NULL;
    char *relative_path = NULL;
    char *candidate_path = NULL;
    char *file_path = NULL;
    char *body = NULL;
    const char *content_type = NULL;
    enum file_type type = FILE_TYPE_UNSUPPORTED;
    FILE *file = NULL;
    struct stat file_info;

    //get path of document root
    root_path = realpath(document_root, NULL);
    if (root_path == NULL)
    {
        goto not_found;
    }

    //clean leading /, turn "/" into ""
    if (request->fileName == NULL)
    {
        goto not_found;
    }
    char *uri_path = request->fileName;
    while (*uri_path == '/')
    {
        uri_path++;
    }

    //edge case for "/"
    size_t path_length = strcspn(uri_path, "?");
    if (path_length == 0)
    {
        uri_path = "index.html";
        path_length = strlen(uri_path);
    }

    //copy uri_path to relative_path, make relative_path a valid string
    relative_path = malloc(path_length + 1);
    if (relative_path == NULL)
    {
        goto not_found;
    }
    memcpy(relative_path, uri_path, path_length);
    relative_path[path_length] = '\0';

    //write absolute path of file
    size_t candidate_length = strlen(root_path) + path_length + 2;
    candidate_path = malloc(candidate_length);
    if (candidate_path == NULL)
    {
        goto not_found;
    }
    snprintf(candidate_path, candidate_length, "%s/%s", root_path, relative_path);

    //make sure thats real
    file_path = realpath(candidate_path, NULL);
    if (file_path == NULL)
    {
        goto not_found;
    }

    //check for escape from docRoot
    size_t root_length = strlen(root_path);
    if (strcmp(root_path, "/") != 0 &&
        (strncmp(file_path, root_path, root_length) != 0 ||
         (file_path[root_length] != '/' && file_path[root_length] != '\0')))
    {
        request->returnCode = 403;
        goto cleanup;
    }

    //open file
    file = fopen(file_path, "rb");
    if (file == NULL || fstat(fileno(file), &file_info) != 0 ||
        !S_ISREG(file_info.st_mode) || file_info.st_size < 0 || file_info.st_size > INT_MAX)
    {
        goto not_found;
    }

    const char *file_name = strrchr(relative_path, '/');
    file_name = file_name == NULL ? relative_path : file_name + 1;
    const char *extension = strrchr(file_name, '.');
    if (extension != NULL && strcasecmp(extension, ".html") == 0)
    {
        type = FILE_TYPE_HTML;
    }
    else if (extension != NULL && strcasecmp(extension, ".htm") == 0)
    {
        type = FILE_TYPE_HTML;
    }
    else if (extension != NULL &&
             (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0))
    {
        type = FILE_TYPE_JPG;
    }
    else if (extension != NULL && strcasecmp(extension, ".gif") == 0)
    {
        type = FILE_TYPE_GIF;
    }

    switch (type)
    {
    case FILE_TYPE_HTML:
        content_type = "text/html";
        break;
    case FILE_TYPE_JPG:
        content_type = "image/jpeg";
        break;
    case FILE_TYPE_GIF:
        content_type = "image/gif";
        break;
    default:
        request->returnCode = 415;
        goto cleanup;
    }
    
    //save data of file
    body = malloc((size_t)file_info.st_size + 1);
    if (body == NULL || fread(body, 1, (size_t)file_info.st_size, file) != (size_t)file_info.st_size)
    {
        goto not_found;
    }
    body[file_info.st_size] = '\0';

    //save to request
    request->messageBody = body;
    request->contentLength = (int)file_info.st_size;
    request->contentType = (char *)content_type;
    request->returnCode = 200;
    body = NULL;

    //close file and free
    fclose(file);
    file = NULL;
    free(file_path);
    free(candidate_path);
    free(relative_path);
    free(root_path);
    return;

not_found:
    request->returnCode = 404;
cleanup:
    if (file != NULL)
    {
        fclose(file);
    }
    free(body);
    free(file_path);
    free(candidate_path);
    free(relative_path);
    free(root_path);
}



const char *ERROR_400_HTML =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>400 Bad Request</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>400 Bad Request</h1>\n"
    "    <p>The server could not understand the request due to invalid syntax.</p>\n"
    "</body>\n"
    "</html>";

const char *ERROR_403_HTML =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>403 Forbidden</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>403 Forbidden</h1>\n"
    "    <p>You do not have permission to access this resource on this server.</p>\n"
    "</body>\n"
    "</html>";

const char *ERROR_404_HTML =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>404 Not Found</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>404 Not Found</h1>\n"
    "    <p>The requested resource could not be found on this server.</p>\n"
    "</body>\n"
    "</html>";

const char *ERROR_415_HTML =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>415 Unsupported Media Type</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>415 Unsupported Media Type</h1>\n"
    "</body>\n"
    "</html>";

const char *get_status_text(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 415:
        return "Unsupported Media Type";
    default:
        return "Unknown";
    }
}

void get_date_string(char *buffer, int size)
{
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    strftime(buffer, size, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
}

void generate_headers(struct httpRequest *request)
{
    char date[128];
    char header_buffer[BUFSIZE];

    get_date_string(date, sizeof(date));

    const char *status_text = get_status_text(request->returnCode);

    request->headers = malloc(BUFSIZE);

    if (request->messageBody == NULL)
    {
        request->messageBody = "";
    }
    if (request->contentType == NULL)
    {
        request->contentType = "text/html";
    }

    switch (request->returnCode)
    {
    case 400:
        request->messageBody = strdup(ERROR_400_HTML);
        request->contentType = "text/html";
        request->contentLength = strlen(request->messageBody);
        break;
    case 403:
        request->messageBody = strdup(ERROR_403_HTML);
        request->contentType = "text/html";
        request->contentLength = strlen(request->messageBody);
        break;
    case 404:
        request->messageBody = strdup(ERROR_404_HTML);
        request->contentType = "text/html";
        request->contentLength = strlen(request->messageBody);
        break;
    case 415:
        request->messageBody = strdup(ERROR_415_HTML);
        request->contentType = "text/html";
        request->contentLength = strlen(request->messageBody);
        break;
    default:
        break;
    }

    snprintf(request->headers, BUFSIZE,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Date: %s\r\n"
             "\r\n",
             request->returnCode,
             status_text,
             request->contentType,
             request->contentLength,
             date);
}