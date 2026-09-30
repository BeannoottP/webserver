/* 
 * bp14
 * a basic webserver in c
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
#include "request.c"

int main(int argc, char **argv) {
    int listenfd;
    int *connfd;
    int portno;
    socklen_t clientlen;
    pthread_t tid;
    int optval;

    struct sockaddr_in myaddr;
    struct sockaddr clientaddr;

    // check command line args
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);

    myaddr.sin_port = htons(portno);
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //chatgpt gave me this line
    printf("%s\n", inet_ntoa(myaddr.sin_addr));


    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd < 0) {
        printf("ERROR opening socket\n");
        exit(1);
    }

    //some jeannie bs
    optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    if (bind(listenfd, (struct sockaddr*) &myaddr, sizeof(myaddr)) < 0) {
        printf("Error on binding\n");
        exit(1);
    }

    if (listen(listenfd, 10) < 0) {
        printf("Error on listen \n");
        exit(1);
    }

    while (1) {

        clientlen = sizeof(clientaddr);

        connfd = malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        if (connfd < 0) {
            printf("Error on accept \n");
            exit(1);
        }
        
        printf("Client Connected\n");

        if (pthread_create(&tid, NULL, run_http_thread, connfd) != 0) {
            printf("error creating threads\n");
            exit(1);
        }


    }

    return 0;
}