#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ChainedList.h"
#include "command.h"
#include "user.h"
#include "cJSON.h"
#include "file.h"
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

List *client_sockets;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void sendAllClients(const char *message);
void sendClient(int socket_fd, const char *message);
void removeClient(int socket_fd);
void addClient(int sock);
void *handleClient(void *arg);
void sendFileContent(int client, const char *filename);
void upload(int sock, const char *filename);
void download(int sock, const char *filename);
void create_directory(const char *dir);
void handle_login(int client_socket);

void addClient(int sock)
{
    pthread_mutex_lock(&clients_mutex);
    addLast(client_sockets, sock);
    pthread_mutex_unlock(&clients_mutex);
}

void removeClient(int socket_fd)
{
    pthread_mutex_lock(&clients_mutex);

    if (!isListEmpty(client_sockets))
    {
        Node *current = client_sockets->first;
        while (current != NULL)
        {
            if (current->val == socket_fd)
            {
                close(socket_fd);
                break;
            }
            current = current->next;
        }
        removeElement(client_sockets, socket_fd);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void sendClient(int socket_fd, const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (isListEmpty(client_sockets))
    {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    Node *current = client_sockets->first;
    while (current != NULL)
    {
        if (current->val == socket_fd)
        {
            send(socket_fd, message, strlen(message) + 1, 0);
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

void sendAllClients(const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (isListEmpty(client_sockets))
    {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    Node *current = client_sockets->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handleClient(void *arg)
{
    int sock = *((int *)arg);
    free(arg);

    handle_login(sock);

    char buffer[MAX_MESSAGE_SIZE];

    while (1)
    {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0)
        {
            break;
        }

        buffer[received] = '\0';

        // Order processing
        if (strcmp(buffer, "@help") == 0)
        {
            sendFileContent(sock, "README.txt");
            continue;
        }

        if (strcmp(buffer, "@credits") == 0)
        {
            sendFileContent(sock, "Credits.txt");
            continue;
        }

        if (strncmp(buffer, "@upload", 7) == 0)
        {
            char filename[100];
            if (sscanf(buffer + 8, "%s", filename) == 1)
            {
                upload(sock, filename);
            }
            else
            {
                send(sock, "Nom de fichier manquant.\n", 26, 0);
            }
            continue;
        }

        if (strncmp(buffer, "@download ", 10) == 0)
        {
            download(sock, buffer);
        }

        else
        {
            const char *msg = "Commande inconnue.\n";
            send(sock, msg, strlen(msg), 0);
        }
    }

    printf("Client %d déconnecté\n", sock);
    removeClient(sock);
    return NULL;
}

void sendFileContent(int client, const char *filename)
{
    FileHandler file = open_file(filename, "r");

    char ligne[1024];
    while (read_file(&file, ligne, sizeof(ligne)) == 1)
    {
        send(client, ligne, strlen(ligne), 0);
        send(client, "\n", 1, 0);
    }

    close_file(&file);
    send(client, "__END__", 7, 0);
}

void create_directory(const char *dir)
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

// reçoit un fichier envoyé par un client via le socket et le sauvegarde localement
void upload(int sock, const char *filename)
{
    // sécurité: éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL)
    {
        send(sock, "Nom de fichier invalide.\n", 26, 0);
        return;
    }

    printf("Démarrage de l'upload du fichier: %s\n", filename);

    // Création du répertoire uploads s'il n'existe pas
    create_directory("uploads");
    printf("Répertoire 'uploads' vérifié.\n");

    // Préfixer le chemin du fichier avec "uploads/"
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);
    printf("Sauvegarde vers: %s\n", filepath);

    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier pour écriture");
        send(sock, "Erreur serveur: impossible de créer le fichier.\n", 48, 0);
        return;
    }

    char buffer[1024]; // pour stocker les données reçues du client
    int len;
    int total = 0;

    printf("En attente des données...\n");
    while ((len = recv(sock, buffer, sizeof(buffer), 0)) > 0)
    {
        if (len == 7 && strncmp(buffer, "__END__", 7) == 0)
        { // détection du marqueur de fin
            printf("Marqueur de fin détecté.\n");
            break;
        }

        total += len;
        printf("Reçu %d octets (total: %d)\n", len, total);
        fwrite(buffer, 1, len, fp);
        fflush(fp);
    }

    fclose(fp);
    printf("Fichier reçu et sauvegardé: %s (%d octets)\n", filepath, total);
    send(sock, "Fichier reçu avec succès\n", 26, 0);
}

void download(int client_socket, const char *input)
{
    char filename[256] = {0};
    char filepath[512] = {0};
    char response[1024] = {0};

    printf("Commande download reçue: %s\n", input);

    // Extraire le nom du fichier
    if (sscanf(input + 10, "%255s", filename) != 1)
    {
        strcpy(response, "Erreur: Format incorrect. Utilisation: @download nom_fichier\n");
        send(client_socket, response, strlen(response), 0);
        return;
    }

    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);

    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        sprintf(response, "Erreur: Le fichier '%s' n'existe pas dans le répertoire uploads.\n", filename);
        send(client_socket, response, strlen(response), 0);
        return;
    }

    // Obtenir la taille du fichier
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);

    // Informer le client que le fichier va être envoyé
    sprintf(response, "READY_TO_SEND:%s:%ld", filename, file_size);
    printf("Envoi de la réponse: %s\n", response);
    send(client_socket, response, strlen(response), 0);

    // Attente de confirmation
    char confirm[32] = {0};
    int confirm_recv = recv(client_socket, confirm, sizeof(confirm), 0);
    if (confirm_recv <= 0)
    {
        printf("Erreur: Pas de confirmation du client\n");
        return;
    }

    if (strcmp(confirm, "READY") == 0)
    {
        FILE *f = fopen(filepath, "rb");
        if (!f)
        {
            strcpy(response, "Erreur: Impossible d'ouvrir le fichier pour l'envoi.\n");
            send(client_socket, response, strlen(response), 0);
            return;
        }

        char buffer[1024];
        size_t bytes_read;
        long sent = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
        {
            int bytes_sent = send(client_socket, buffer, bytes_read, 0);
            if (bytes_sent <= 0)
            {
                printf("Erreur d'envoi: %d\n", bytes_sent);
                break;
            }
            sent += bytes_sent;
            printf("\rEnvoi: %ld/%ld octets (%.1f%%)", sent, file_size, (float)sent / file_size * 100);
            fflush(stdout);
        }

        fclose(f);

        if (sent == file_size)
        {
            sprintf(response, "Fichier '%s' (%ld octets) envoyé avec succès.\n", filename, file_size);
        }
        else
        {
            sprintf(response, "Erreur: Transfert incomplet. Envoyé %ld/%ld octets.\n", sent, file_size);
        }

        printf("%s", response);
        send(client_socket, response, strlen(response), 0);
    }
    else
    {
        strcpy(response, "Erreur: Le client n'est pas prêt à recevoir le fichier.\n");
        send(client_socket, response, strlen(response), 0);
    }
}

void handle_login(int client_socket)
{
    char buffer[200];
    char pseudo[100];
    char mdp[100];

    // Demander pseudo
    send(client_socket, "Entrez votre pseudo: ", 22, 0);
    int len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0)
        return;
    buffer[len] = '\0';
    strncpy(pseudo, buffer, sizeof(pseudo));

    // Demander mot de passe
    send(client_socket, "Entrez votre mot de passe: ", 28, 0);
    len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0)
        return;
    buffer[len] = '\0';
    strncpy(mdp, buffer, sizeof(mdp));

    // Vérifier si déjà enregistré
    FileHandler f = open_file("save_users.txt", "r");
    int exists = user_exists(&f, pseudo);
    close_file(&f);

    if (!exists)
    {
        save_user(pseudo, mdp);
        send(client_socket, "Utilisateur enregistré avec succès.\n", 39, 0);
    }
    else
    {
        send(client_socket, "Utilisateur déjà enregistré.\n", 33, 0);
    }
}

int main()
{
    int dSock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket creation
    loadUsersFromJson("users.json");

    if (dSock == -1)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(dSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) // allow reuse of the address
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = bind(dSock, (struct sockaddr *)&ad, sizeof(ad)); // bind the socket to the address
    if (res == -1)
    {
        perror("bind");
        exit(1);
    }

    res = listen(dSock, 5); // listen on the socket
    if (res == -1)
    {
        perror("listen");
        exit(1);
    }

    client_sockets = (List *)malloc(sizeof(List));
    if (client_sockets == NULL)
    {
        perror("malloc client_sockets");
        exit(1);
    }
    client_sockets->first = NULL;
    client_sockets->curr = NULL;
    client_sockets->size = 0;

    printf("Serveur démarré sur le port 31473\n");

    while (1)
    {
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);

        int new_socket = accept(dSock, (struct sockaddr *)&clientaddr, &addrlen);

        if (new_socket < 0)
        {
            perror("accept");
            continue;
        }

        printf("Nouveau client connecté (socket: %d)\n", new_socket);
        addClient(new_socket);

        int *arg = malloc(sizeof(int));
        if (arg == NULL)
        {
            perror("malloc");
            removeClient(new_socket);
            continue;
        }
        *arg = new_socket;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleClient, arg) != 0)
        {
            perror("pthread_create");
            free(arg);
            removeClient(new_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    free(client_sockets);
    close(dSock);
    pthread_mutex_destroy(&clients_mutex);
    return 0;
}
