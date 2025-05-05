#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define MAX_MESSAGE_SIZE 2000

void* handle_client(void* client_sock_ptr) {
    int sock = *((int*)client_sock_ptr);
    free(client_sock_ptr);
    
    char buffer[MAX_MESSAGE_SIZE];
    
    while(1) {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break;
        }
        
        buffer[received] = '\0';
        printf("Message: %s\n", buffer);
        
        send(sock, "Reçu", 5, 0);
    }
    
    close(sock);
    return NULL;
}

int main()
{
    int dSock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket creation
    if (dSock == -1)
    {
        perror("socket");
        exit(1);
    }

    // Permettre la réutilisation de l'adresse
    int opt = 1;
    if (setsockopt(dSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = bind(dSock, (struct sockaddr *)&ad, sizeof(ad));

    if (res == -1)
    {
        perror("bind");
        exit(1);
    }
    res = listen(dSock, 5); // listen on the socket
    
   
    while(1) {
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(dSock, (struct sockaddr*)&clientaddr, &addrlen);
        
        if (*client_sock < 0) {
            perror("accept");
            free(client_sock);
            continue;
        }
        
        printf("Nouveau client connecté\n");
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_sock) != 0) {
            perror("pthread_create");
            close(*client_sock);
            free(client_sock);
            continue;
        }
        
        pthread_detach(thread_id);
    }


    return 0;
}