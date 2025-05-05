#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_MESSAGE_SIZE 2000

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
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = connect(dSock, (struct sockaddr *)&ad, sizeof(ad)); // connect to the server
    if (res == -1)
    {
        perror("connect");
        exit(1);
    }

    // Rendre le socket non bloquant
    int flags = fcntl(dSock, F_GETFL, 0);
    fcntl(dSock, F_SETFL, flags | O_NONBLOCK);

    char message[MAX_MESSAGE_SIZE+1];
    char buffer[1024];
    
    while(1) {
        printf("Entrez une chaîne (max %d caractères): ", MAX_MESSAGE_SIZE);
        if (fgets(message, MAX_MESSAGE_SIZE+1, stdin) == NULL)
        {
            perror("fgets");
            continue;
        }
        
        message[strcspn(message, "\n")] = 0;
        printf("Envoi du message: %s\n", message);
        
        int bytesSent = send(dSock, message, strlen(message) + 1, 0);
        if (bytesSent == -1)
        {
            perror("send");
            continue;
        }
        
        // Vérifier s'il y a des données à lire (non bloquant)
        int bytesRead = recv(dSock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            printf("Message reçu: %s\n", buffer);
        }
        else if (bytesRead == 0) {
            printf("Connexion fermée par le serveur\n");
            break;
        }
        else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
                break;
            }
            // Aucune donnée disponible pour le moment, on continue
        }
    }
    
    close(dSock); // close the socket
    return 0;
}