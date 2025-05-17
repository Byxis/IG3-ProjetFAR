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
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

List *client_sockets;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
extern pthread_mutex_t users_mutex;

void sendAllClients(const char *message);
void send_client(int socket_fd, const char *message);
void remove_client(int socket_fd);
void add_client(int socket_fd);
void *handle_client(void *arg);
void sendFileContent(int client, const char *filename);
void upload(int socketFd, const char *filename);
void download(int socketFd, const char *input);
void create_directory(const char *dir);
void handle_login(int client_socket);

void add_client(int socket_fd)
{
    pthread_mutex_lock(&clients_mutex);
    addLast(client_sockets, socket_fd);
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket_fd)
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

void send_client(int socket_fd, const char *message)
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

void *handle_client(void *arg)
{
    int socket_fd = *((int *)arg);
    free(arg);
    handle_login(socket_fd);
    char buffer[MAX_MESSAGE_SIZE];

    while (1)
    {
        int received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0)
        {
            break;
        }
        buffer[received] = '\0';
        printf("Client %d: %s\n", socket_fd, buffer);
        executeCommand(socket_fd, buffer);
    }
    printf("Client %d déconnecté\n", socket_fd);
    remove_client(socket_fd);
    return NULL;
}

void sendFileContent(int client, const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file)
        return;

    char line[1024];

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';
        send(client, line, strlen(line), 0);
        send(client, "\n", 1, 0);
    }
    fclose(file);
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

void upload(int socketFd, const char *filename)
{
    if (strstr(filename, "..") != NULL)
    {
        send(socketFd, "Nom de fichier invalide.\n", 26, 0);
        return;
    }

    printf("Démarrage de l'upload du fichier: %s\n", filename);
    create_directory("uploads");
    printf("Répertoire 'uploads' vérifié.\n");

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);
    printf("Sauvegarde vers: %s\n", filepath);
    FILE *fp = fopen(filepath, "wb");

    if (fp == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier pour écriture");
        send(socketFd, "Erreur serveur: impossible de créer le fichier.\n", 48, 0);
        return;
    }
    char buffer[1024];
    int len;
    int total = 0;
    printf("En attente des données...\n");

    while ((len = recv(socketFd, buffer, sizeof(buffer), 0)) > 0)
    {
        if (len == 7 && strncmp(buffer, "__END__", 7) == 0)
        {
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
    send(socketFd, "Fichier reçu avec succès\n", 26, 0);
}

void download(int socketFd, const char *input)
{
    char filename[256] = {0};
    char filepath[512] = {0};
    char response[1024] = {0};
    printf("Commande download reçue: %s\n", input);

    if (sscanf(input + 10, "%255s", filename) != 1)
    {
        strcpy(response, "Erreur: Format incorrect. Utilisation: @download nom_fichier\n");
        send(socketFd, response, strlen(response), 0);
        return;
    }

    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);
    FILE *file = fopen(filepath, "rb");

    if (!file)
    {
        sprintf(response, "Erreur: Le fichier '%s' n'existe pas dans le répertoire uploads.\n", filename);
        send(socketFd, response, strlen(response), 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);
    sprintf(response, "READY_TO_SEND:%s:%ld", filename, file_size);
    printf("Envoi de la réponse: %s\n", response);
    send(socketFd, response, strlen(response), 0);

    char confirm[32] = {0};
    int confirm_recv = recv(socketFd, confirm, sizeof(confirm), 0);

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
            send(socketFd, response, strlen(response), 0);
            return;
        }

        char buffer[1024];
        size_t bytes_read;
        long sent = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
        {
            int bytes_sent = send(socketFd, buffer, bytes_read, 0);
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
        send(socketFd, "__END__", 7, 0);

        if (sent == file_size)
        {
            sprintf(response, "Fichier '%s' (%ld octets) envoyé avec succès.\n", filename, file_size);
        }
        else
        {
            sprintf(response, "Erreur: Transfert incomplet. Envoyé %ld/%ld octets.\n", sent, file_size);
        }
        send(socketFd, response, strlen(response), 0);
    }
    else
    {
        strcpy(response, "Erreur: Le client n'est pas prêt à recevoir le fichier.\n");
        send(socketFd, response, strlen(response), 0);
    }
}

void handle_login(int client_socket)
{
    char buffer[200];
    char username[100];
    char password[100];
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    getpeername(client_socket, (struct sockaddr *)&addr, &len);
    send(client_socket, "Entrez votre pseudo: ", 22, 0);

    int rlen = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (rlen <= 0)
        return;

    buffer[rlen] = '\0';
    strncpy(username, buffer, sizeof(username));
    username[sizeof(username) - 1] = '\0';

    send(client_socket, "Entrez votre mot de passe: ", 28, 0);
    rlen = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (rlen <= 0)
        return;

    buffer[rlen] = '\0';
    strncpy(password, buffer, sizeof(password));
    password[sizeof(password) - 1] = '\0';

    User *user = findUserByName(username);

    if (!user)
    {
        registerUser(username, password, client_socket, addr);
    }
    else if (strcmp(user->password, password) == 0)
    {
        pthread_mutex_lock(&users_mutex);
        user->authenticated = true;
        user->socket_fd = client_socket;
        pthread_mutex_unlock(&users_mutex);
        send(client_socket, "Connexion réussie.\n", 20, 0);
    }
    else
    {
        send(client_socket, "Mot de passe incorrect.\n", 25, 0);
    }
}

int main()
{
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    loadUsersFromJson("users.json");

    if (server_socket == -1)
    {
        perror("socket");
        exit(1);
    }
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((short)31473);

    int res = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));

    if (res == -1)
    {
        perror("bind");
        exit(1);
    }

    res = listen(server_socket, 5);

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
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addrlen);

        if (new_socket < 0)
        {
            perror("accept");
            continue;
        }

        printf("Nouveau client connecté (socket: %d)\n", new_socket);
        add_client(new_socket);

        int *arg = malloc(sizeof(int));

        if (arg == NULL)
        {
            perror("malloc");
            remove_client(new_socket);
            continue;
        }

        *arg = new_socket;
        pthread_t thread_id;

        if (pthread_create(&thread_id, NULL, handle_client, arg) != 0)
        {
            perror("pthread_create");
            free(arg);
            remove_client(new_socket);
            continue;
        }
        pthread_detach(thread_id);
    }
    free(client_sockets);
    close(server_socket);
    pthread_mutex_destroy(&clients_mutex);
    return 0;
}
