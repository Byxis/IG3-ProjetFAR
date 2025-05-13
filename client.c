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
#include <sys/stat.h> 



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

            if (strcmp(buffer, "__END__") == 0) {
                continue;
            }

            printf("%s", buffer); 
            fflush(stdout);
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

// lit un fichier local en binaire et l'envoie au serveur 
void upload(int sock, const char *filename)
{
    // sécurité : éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL)
    {
        printf("Nom de fichier invalide.\n");
        return;
    }

    // Vérifier si le fichier existe avant de tenter de l'ouvrir
    FILE *check = fopen(filename, "rb");
    if (check == NULL)
    {
        printf("Erreur: Impossible d'ouvrir le fichier %s\n", filename);
        return;
    }
    fclose(check);

    FileHandler file = open_file(filename, "rb"); // ouverture du fichier en lecture binaire
    if (file.fp == NULL)
    {
        printf("Erreur: Impossible d'ouvrir le fichier pour lecture\n");
        return;
    }

    // Envoie de la commande d'upload
    char command[256];
    sprintf(command, "@upload %s", filename);
    send(sock, command, strlen(command) + 1, 0);

    // Attendre un peu pour s'assurer que la commande est traitée
    usleep(100000); 

    // Ensuite envoyer le contenu du fichier
    char buffer[1024]; // stock du blocs de fichiers lus
    size_t bytes;      // stock du nombre d'octets lus

    printf("Envoi du fichier %s...\n", filename);
    while ((bytes = fread(buffer, 1, sizeof(buffer), file.fp)) > 0)
    {
        send(sock, buffer, bytes, 0);
        // Attendre un peu pour éviter la congestion
        usleep(10000);
    }

    // Marquer la fin du fichier
    usleep(100000); // Attendre un peu avant d'envoyer le marqueur de fin
    send(sock, "__END__", 7, 0);
    printf("Fichier envoyé avec succès!\n");

    close_file(&file);
}


void create_directory(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) != 0) {
            perror("Erreur lors de la création du répertoire");
            return; 
        }
    }
}




// lit un fichier distant et le télécharge dans le répertoire local
void download_file(int server_socket, const char *filename) {
    // Sécurité : éviter les chemins relatifs
    if (strstr(filename, "..") != NULL) {
        printf("Nom de fichier invalide.\n");
        return;
    }
    // Envoie de la commande @download
    char command[300];
    snprintf(command, sizeof(command), "@download %s", filename);
    send(server_socket, command, strlen(command), 0);

    char response[1024] = {0};
    int len = recv(server_socket, response, sizeof(response) - 1, 0);
    if (len <= 0) {
        perror("Erreur de réception initiale");
        return;
    }
    response[len] = '\0';

    char server_filename[256];
    long filesize;

    // Vérification de la structure du message READY_TO_SEND
    if (sscanf(response, "READY_TO_SEND:%255[^:]:%ld", server_filename, &filesize) != 2) {
        printf("Erreur: Réponse inattendue du serveur: %s\n", response);
        return;
    }

    printf("Téléchargement de '%s' (%ld octets) depuis le serveur...\n", server_filename, filesize);

    // Envoie de la confirmation 
    send(server_socket, "READY", 5, 0);

    // Création du dossier "downloads" si nécessaire
    char localpath[300];
    snprintf(localpath, sizeof(localpath), "downloads/%s", server_filename);
    create_directory("downloads");

    FILE *file = fopen(localpath, "wb");
    if (!file) {
        perror("Erreur lors de la création du fichier local");
        return;
    }

    // Réception du contenu du fichier
    char buffer[1024];
    long received = 0;
    while (received < filesize) {
        int n = recv(server_socket, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            printf("Erreur lors du transfert. Reçu %ld/%ld octets.\n", received, filesize);
            break;
        }
        fwrite(buffer, 1, n, file);
        received += n;
        printf("\rRéception: %ld/%ld octets (%.1f%%)", received, filesize, (float)received / filesize * 100);
        fflush(stdout);
    }

    fclose(file);

    if (received == filesize) {
        printf("\n Fichier '%s' téléchargé avec succès dans 'downloads/'.\n", server_filename);
    } else {
        printf("\nTéléchargement incomplet : %ld/%ld octets.\n", received, filesize);
    }

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
    ad.sin_addr.s_addr = INADDR_ANY; 
    ad.sin_port = htons((short)31473);

    int res = connect(dSock, (struct sockaddr *)&ad, sizeof(ad));
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
    char buffer[256];

    // Lecture du pseudo
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    send(dSock, buffer, strlen(buffer), 0);

    // Lecture du mot de passe
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;
    send(dSock, buffer, strlen(buffer), 0);

    // Attente réponse du serveur
    int len = recv(dSock, buffer, sizeof(buffer) - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        printf("%s\n", buffer);
    }


    while (1)
    {

        if (fgets(message, MAX_MESSAGE_SIZE + 1, stdin) == NULL)
        {
            perror("fgets");
            continue;
        }
        
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "@help") == 0 || strcmp(message, "@credits") == 0) {
            send(dSock, message, strlen(message), 0);
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

        if (strncmp(message, "@download ", 10) == 0) {
            char filename[100];
            if (sscanf(message + 10, "%s", filename) == 1) {
                download_file(dSock, filename);
            } else {
                printf("Nom de fichier manquant.\n");
            }
            continue;
        }
        else {
            printf("Commande inconnue. Essayez @help, @upload <fichier>, @download <fichier>, @credits\n");
        }

}}