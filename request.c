/*
A collection of functions to handle requests
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>

#define BUFSIZE 1024
#define MAXMESSAGES 16

// void generate_headers() {}
// void
struct httpRequest {
    int requestVersion; // 0 if 1.0, 1 if 1.1
    char* message; // first line of message
    char* fileName; // middle name of file
    int returnCode; 
    char* headers;
    char* messageBody;
    char* contentType;
    int contentLength;
};

void* run_http_thread(void *vargp)
{
    char message[BUFSIZE];
    int num_read;                 /* num bytes read */
    int num_sent;                 /* num bytes sent */
    int connfd = *((int *)vargp); /* nasty pointer casting. this is our client fd */

    char *messageLines[MAXMESSAGES];
    int numMessages = 0;

    /* detach this thread from parent thread */
    if (pthread_detach(pthread_self()) != 0)
    {
        printf("error detaching\n");
        exit(1);
    }

    /* free heap space for vargp since we have connfd */
    free(vargp);

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

    //create empty http request
    struct httpRequest *request = malloc(sizeof(struct httpRequest));
    request->returnCode = 200;
    request->contentType = "text/html";
    request->messageBody = "<h1>oh my god</h1>";
    request->contentLength = strlen(request->messageBody);

    generate_headers(request);
    send(connfd, request->headers, strlen(request->headers), 0);
    send(connfd, request->messageBody, request->contentLength, 0);

    shutdown(connfd, 0);
    close(connfd);
    return NULL;
}

const char* get_status_text(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        default:  return "Unknown";
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
        date
    );
}