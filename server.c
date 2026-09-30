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
    int portno = 0;
    socklen_t clientlen;
    pthread_t tid;
    int optval;
    char *endptr;


    struct sockaddr_in myaddr;
    struct sockaddr clientaddr;

    long parsed_port;
    char *document_root = NULL;
    int have_document_root = 0;
    int have_port = 0;


    if (argc != 5)
    {
        goto invalid_args;
    }

    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-document_root") == 0 && !have_document_root && argv[i + 1][0] != '\0')
        {
            document_root = argv[i + 1];
            have_document_root = 1;
        }
        else if (strcmp(argv[i], "-port") == 0 && !have_port)
        {
            parsed_port = strtol(argv[i + 1], &endptr, 10);
            if (endptr == argv[i + 1] || *endptr != '\0' || parsed_port < 1 || parsed_port > 65535)
            {
                goto invalid_args;
            }
            portno = (int)parsed_port;
            have_port = 1;
        }
        else
        {
            goto invalid_args;
        }
    }

    if (!have_document_root || !have_port)
    {
        goto invalid_args;
    }

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

        int accepted_fd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

        if (accepted_fd < 0) {
            printf("Error on accept \n");
            continue;
        }
        
        printf("Client Connected\n");

        struct thread_args *args = malloc(sizeof *args);
        if (args == NULL) {
            close(accepted_fd);
            fprintf(stderr, "error allocating thread arguments\n");
            continue;
        }

        args->connfd = accepted_fd;
        args->document_root = document_root;

        if (pthread_create(&tid, NULL, run_http_thread, args) != 0) {
            printf("error creating threads\n");
            close(accepted_fd);
            free(args);
        }


    }

    return 0;

invalid_args:
    fprintf(stderr, "usage: %s -document_root <directory> -port <port>\n", argv[0]);
    return 1;
}