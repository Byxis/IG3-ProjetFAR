#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 2000

int inDownload = 0;

/**
 ** Thread function to receive and print messages from the server.
 * @param socketPtr (void*) - Pointer to the socket file descriptor.
 * @returns void*
 */
void *receiveMessages(void *socketPtr)
{
    int socketFd = *((int *)socketPtr);
    char buffer[1024];

    while (1)
    {
        int bytesRead = recv(socketFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            if (strcmp(buffer, "__END__") == 0)
            {
                continue;
            }
            if (strcmp(buffer, "Fichier reçu avec succès\n") == 0 || strcmp(buffer, "Fichier reçu avec succès") == 0)
            {
                continue;
            }
            printf("%s\n", buffer);
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

/**
 ** Send a local file in binary mode to the server.
 * @param socketFd (int) - The server socket file descriptor.
 * @param filename (const char*) - The name of the file to upload.
 * @returns void
 */
void uploadFile(int socketFd, const char *filename)
{
    if (strstr(filename, "..") != NULL)
    {
        printf("Nom de fichier invalide.\n");
        return;
    }
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("Erreur: Impossible d'ouvrir le fichier %s\n", filename);
        return;
    }

    // Obtenir la taille du fichier
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char command[256];
    snprintf(command, sizeof(command), "@upload %s", filename);
    send(socketFd, command, strlen(command), 0);
    usleep(100000);

    char buffer[1024];
    size_t bytes;
    size_t total = 0;

    printf("Envoi de '%s' (%ld octets) vers le serveur...\n", filename, filesize);

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        send(socketFd, buffer, bytes, 0);
        total += bytes;
        printf("\rEnvoi: %zu/%ld octets (%.1f%%)", total, filesize, (float)total / filesize * 100);
        fflush(stdout);
        usleep(10000);
    }

    usleep(100000);
    send(socketFd, "__END__", 7, 0);

    if (total == filesize)
    {
        printf("\nFichier '%s' (%ld octets) envoyé avec succès.\n", filename, filesize);
    }
    else
    {
        printf("\nEnvoi incomplet : %zu/%ld octets.\n", total, filesize);
    }

    fclose(file);
}

/**
 ** Create a directory if it does not exist.
 * @param dir (const char*) - The name of the directory to create.
 * @returns void
 */
void createDirectory(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0700) != 0)
        {
            perror("Erreur lors de la création du répertoire");
            return;
        }
    }
}

/**
 ** Download a remote file from the server and save it locally.
 * @param socketFd (int) - The server socket file descriptor.
 * @param filename (const char*) - The name of the file to download.
 * @returns void
 */
void downloadFile(int socketFd, const char *filename)
{
    extern int inDownload;
    inDownload = 1;

    if (strstr(filename, "..") != NULL)
    {
        printf("Nom de fichier invalide.\n");
        inDownload = 0;
        return;
    }

    char command[300];
    snprintf(command, sizeof(command), "@download %s", filename);
    send(socketFd, command, strlen(command), 0);

    char response[1024] = {0};
    int len = recv(socketFd, response, sizeof(response) - 1, 0);

    if (len <= 0)
    {
        perror("Erreur de réception initiale");
        inDownload = 0;
        return;
    }
    response[len] = '\0';
    char serverFilename[256];
    long filesize;

    if (sscanf(response, "READY_TO_SEND:%255[^:]:%ld", serverFilename, &filesize) != 2)
    {
        printf("Erreur: Réponse inattendue du serveur: %s\n", response);
        inDownload = 0;
        return;
    }
    printf("Téléchargement de '%s' (%ld octets) depuis le serveur...\n", serverFilename, filesize);
    send(socketFd, "READY", 5, 0);

    char localPath[300];
    snprintf(localPath, sizeof(localPath), "downloads/%s", serverFilename);
    createDirectory("downloads");
    FILE *file = fopen(localPath, "wb");

    if (!file)
    {
        perror("Erreur lors de la création du fichier local");
        inDownload = 0;
        return;
    }

    char buffer[1024];
    long received = 0;

    while (received < filesize)
    {
        int n = recv(socketFd, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            printf("Erreur lors du transfert. Reçu %ld/%ld octets.\n", received, filesize);
            break;
        }
        long toWrite = n;
        if (received + n > filesize)
        {
            toWrite = filesize - received;
        }
        if (received + toWrite == filesize)
        {
            int extra = n - toWrite;
            if (extra > 0)
            {
                if (memcmp(buffer + toWrite, "__END__", 7) == 0)
                {
                    n = toWrite;
                }
            }
        }
        fwrite(buffer, 1, toWrite, file);
        received += toWrite;
        printf("\rRéception: %ld/%ld octets (%.1f%%)", received, filesize, (float)received / filesize * 100);
        fflush(stdout);
        if (received >= filesize)
        {
            break;
        }
    }

    fclose(file);
    inDownload = 0;

    char endMarker[16] = {0};
    int endLen = recv(socketFd, endMarker, sizeof(endMarker) - 1, 0);

    if (endLen > 0 && strstr(endMarker, "__END__") != NULL)
    {
        char confirmMsg[256] = {0};
        int confirmLen = recv(socketFd, confirmMsg, sizeof(confirmMsg) - 1, 0);
        if (confirmLen > 0)
        {
            confirmMsg[confirmLen] = '\0';
            printf("\nFichier '%s' (%ld octets) téléchargé avec succès dans 'downloads/'.\n", serverFilename, filesize);
        }
    }
    else if (endLen > 0)
    {
        endMarker[endLen] = '\0';
        printf("%s", endMarker);
    }
    if (received != filesize)
    {
        printf("\nTéléchargement incomplet : %ld/%ld octets.\n", received, filesize);
    }
}

/**
 ** Main entry point for the client application.
 * @returns int - Exit status code.
 */
int main()
{
    int socketFd = socket(PF_INET, SOCK_STREAM, 0);
    if (socketFd == -1)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((short)31473);

    int res = connect(socketFd, (struct sockaddr *)&addr, sizeof(addr));

    if (res == -1)
    {
        perror("connect");
        exit(1);
    }
    pthread_t threadId;
    if (pthread_create(&threadId, NULL, receiveMessages, &socketFd) != 0)
    {
        perror("pthread_create");
        close(socketFd);
        exit(1);
    }
    char message[MAX_MESSAGE_SIZE + 1];
    char buffer[256];

    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    send(socketFd, buffer, strlen(buffer), 0);
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    send(socketFd, buffer, strlen(buffer), 0);
    int len = recv(socketFd, buffer, sizeof(buffer) - 1, 0);

    if (len > 0)
    {
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
        if (strcmp(message, "@help") == 0 || strcmp(message, "@credits") == 0)
        {
            send(socketFd, message, strlen(message), 0);
            continue;
        }
        if (strncmp(message, "@upload", 7) == 0)
        {
            char filename[100];
            if (sscanf(message + 8, "%s", filename) == 1)
            {
                uploadFile(socketFd, filename);
            }
            else
            {
                send(socketFd, "Nom de fichier manquant.\n", 26, 0);
            }
            continue;
        }
        if (strncmp(message, "@download ", 10) == 0)
        {
            char filename[100];
            if (sscanf(message + 10, "%s", filename) == 1)
            {
                pthread_cancel(threadId);
                pthread_join(threadId, NULL);
                downloadFile(socketFd, filename);
                if (pthread_create(&threadId, NULL, receiveMessages, &socketFd) != 0)
                {
                    perror("pthread_create");
                    close(socketFd);
                    exit(1);
                }
            }
            else
            {
                printf("Nom de fichier manquant.\n");
            }
            continue;
        }
        else
        {
            int bytesSent = send(socketFd, message, strlen(message) + 1, 0);
            if (bytesSent == -1)
            {
                perror("send");
                continue;
            }
        }
    }
}