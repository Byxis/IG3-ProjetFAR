#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h> 
#include "file.h"



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

// lit un fichier local en binaire et l'envoie au serveur via le socket
void upload(int sock, const char *filename) {
    // sécurité : éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL) {
        send(sock, "Nom de fichier invalide.\n", 26, 0);
        return;
    }
    FileHandler file = open_file(filename, "rb"); // ouverture du fichier en lecture binaire 
    char buffer[1024]; // stock du blocs de fichiers lus 
    size_t bytes; // stocke du nombre d'octets lus 
    while ((bytes = fread(buffer, 1, sizeof(buffer), file.fp)) > 0) {
        send(sock, buffer, bytes, 0);
    }
    send(sock, "__END__", 7, 0);
    close_file(&file);
}

// reçoit un fichier depuis le serveur via le socket et le sauvegarde localement
void download(int sock, const char *filename) {
    // sécurité: éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL) {
        send(sock, "Nom de fichier invalide.\n", 26, 0);
        return;
    }
    FileHandler file = open_file(filename, "wb"); // ouvre le fichier pour écriture binaire
    char buffer[1024]; // reception des blocs venant du serveur
    int len;
    while ((len = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (strncmp(buffer, "__END__", 7) == 0 && len == 7) {// détection du marqueur de fin
            break; 
        }
        fwrite(buffer, 1, len, file.fp);
    }
    close_file(&file);
    printf("Fichier '%s' téléchargé avec succès.\n", filename);
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
        
        if (strncmp(message, "@upload", 7) == 0) {
            char filename[100];
            if (sscanf(message + 8, "%s", filename) == 1) {
                upload(dSock, filename);
            }
            else {
                send(dSock, "Nom de fichier manquant.\n", 26, 0);
            }
            continue;
        }
        
        if (strncmp(message, "@download", 9) == 0) {
            char filename[100];
            if (sscanf(message + 10, "%s", filename) == 1) {
                download(dSock, filename);
            }
            else {
                send(dSock, "Nom de fichier manquant.\n", 26, 0);
            }
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