#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h> // Ajout de l'en-tête pthread

#define MAX_MESSAGE_SIZE 2000

void *receiveMessages(void *dS)
{
    int dSock = *((int *)dS);
    char buffer[1024];

    while (1)
    {
        int bytesRead = recv(dSock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            printf("%s\n", buffer);
        }
        else if (bytesRead == 0)
        {
            printf("\nConnexion fermée par le serveur\n");
            exit(0);
        }
        else
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("recv");
                exit(1);
            }
        }
    }

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

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY; // INADDR_ANY or inet_addr("ipv4_address like 1.1.1.1")
    ad.sin_port = htons((short)31473);

    int res = connect(dSock, (struct sockaddr *)&ad, sizeof(ad)); // connect to the server
    if (res == -1)
    {
        perror("connect");
        exit(1);
    }

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, receiveMessages, &dSock) != 0)
    {
        perror("pthread_create");
        close(dSock);
        exit(1);
    }

    char message[MAX_MESSAGE_SIZE + 1];

    printf("Entrez une chaîne (max %d caractères): ", MAX_MESSAGE_SIZE);
    fflush(stdout);

    while (1)
    {
        if (fgets(message, MAX_MESSAGE_SIZE + 1, stdin) == NULL)
        {
            perror("fgets");
            continue;
        }

        message[strcspn(message, "\n")] = 0;

        int bytesSent = send(dSock, message, strlen(message) + 1, 0);
        if (bytesSent == -1)
        {
            perror("send");
            continue;
        }
        fflush(stdout);
    }

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);
    close(dSock);
    return 0;
}