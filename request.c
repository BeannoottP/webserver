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

#define BUFSIZE 1024
#define MAXMESSAGES 16

// void generate_headers() {}
// void
struct httpRequest {
    int requestVersion;
    char* message;
    char* fileName;
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
    

    shutdown(connfd, 0);
    close(connfd);
    return NULL;
}

void generate

void generate_headers(struct httpRequest *request)
{
}

{
    char buf[BUFSIZE];            /* message buffer */
    int num_read;                 /* num bytes read */
    int num_sent;                 /* num bytes sent */
    int connfd = *((int *)vargp); /* nasty pointer casting. this is our client fd */

    /* detach this thread from parent thread */
    if (pthread_detach(pthread_self()) != 0)
    {
        printf("error detaching\n");
        exit(1);
    }

    /* free heap space for vargp since we have connfd */
    free(vargp);

    /* recv: read input string from the client */
    bzero(buf, BUFSIZE);
    num_read = recv(connfd, buf, BUFSIZE, 0);
    if (num_read < 0)
    {
        printf("ERROR reading from socket\n");
        exit(1);
    }
    printf("server received %d bytes: %s\n", num_read, buf);

    /* send: echo the input string back to the client */
    num_sent = send(connfd, buf, num_read, 0);
    if (num_sent < 0)
    {
        printf("ERROR writing to socket\n");
        exit(1);
    }

    /* close client */
    shutdown(connfd, 0);
    close(connfd);
    return NULL;
}