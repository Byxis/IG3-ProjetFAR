#include "command.h"
#include "ChainedList.h"
#include <string.h>
#include <stdlib.h>
#include "user.h"
#include "file.h"

Command parseCommand(const char *msg) {
    if (strncmp(msg, "@command", 5) == 0) return COMMAND;
    if (strncmp(msg, "@help", 5) == 0) return HELP;
    if (strncmp(msg, "@ping", 5) == 0) return PING;
    if (strncmp(msg, "@msg", 4) == 0) return MSG;
    if (strncmp(msg, "@connect", 8) == 0) return CONNECT;
    if (strncmp(msg, "@credits", 8) == 0) return CREDITS;
    if (strncmp(msg, "@shutdown", 9) == 0) return SHUTDOWN;
    if (strncmp(msg, "@create", 9) == 0) return CREATE;
    if (strncmp(msg, "@join", 9) == 0) return JOIN;
    if (strncmp(msg, "@leave", 9) == 0) return LEAVE;
    if (strncmp(msg, "@upload", 9) == 0) return UPLOAD;
    if (strncmp(msg, "@download", 9) == 0) return DOWNLOAD;
}

void sendFileContent(int client, const char*filename) {
    FileHandler file = open_file(filename, "r");

    char ligne[1024];
    while (read_file(&file, ligne, sizeof(ligne)) == 1) {
        send(client, ligne, strlen(ligne), 0);
        send(client, "\n", 1, 0);
    }
    close_file(&file);
}

void privateMessage(int sender_sock, const char* username, const char *msg) {
    User *user = findUserByName(username);
    if (user != NULL) {
        char full_msg[1024];
        snprintf(full_msg, sizeof(full_msg), "[privé] %s", msg);
        send(user->socket_fd, full_msg, strlen(full_msg), 0);
    } else {
        char full_msg[1024];
        snprintf(full_msg, sizeof(full_msg), "Utilisateur '%s' introuvable.", username);
        send(sender_sock,full_msg, strlen(full_msg), 0);
    }
}


void executeCommand(int sock, char* msg, int shutdown) {
    char response[1024];
    Command cmd = parseCommand(msg);

    switch (cmd) {
        case COMMAND : 
            snprintf(response, sizeof(response), 
            "Commandes disponibles:\n"
                     "@help - Affiche l'aide\n"
                     "@ping - Répond 'pong'\n"
                     "@msg <user> <msg> - Message privé\n"
                     "@connect <user> <pwd> - Connexion\n"
                     "@credits - Affiche les crédits\n"
                     "@shutdown - Éteint le serveur\n");
            send(sock, response, strlen(response), 0);
            break;
        
        case PING :
            send(sock, "pong", 4, 0);
            break;

        case MSG :
            char user[MAX_USERNAME_LENGTH]; 
            char message[1024];
            sscanf(msg, "@msg %s %[^\n]", user, message);
            privateMessage(sock, user, message);
            break;
    
        case CONNECT :
            char username[MAX_USERNAME_LENGTH];
            char password[MAX_PASSWORD_LENGTH];
        
            // Vérifie que la commande a bien le bon format
            int parsed = sscanf(msg, "@connect %s %s", username, password);
            if (parsed != 2) {
                send(sock, "Commande invalide. Usage : @connect <username> <password>", 61, 0);
                break;
            }
        
            // Recherche l'utilisateur dans la liste des utilisateurs enregistrés
            User *client = findUserByName(username);
        
            if (client != NULL) {
                // Vérifie le mot de passe
                if (strcmp(client->password, password) == 0) {
                    client->authenticated = true;
                    client->socket_fd = sock;
                    send(sock, "Connexion réussie.", 19, 0);
                } else {
                    send(sock, "Mot de passe incorrect.", 24, 0);
                }
            } else {
                send(sock, "Nom d'utilisateur non trouvé.", 30, 0);
            }
            break;
           
        case HELP :
            sendFileContent(sock, "README.txt");
            break;
        
        case CREDITS :
            sendFileContent(sock, "Credits.txt");
            break;
        
        case SHUTDOWN :
            if (getRoleByName(sock) == ADMIN) {
                send(sock, "Arrêt du serveur...", 21, 0);
                shutdown = 1;
            } else {
                send(sock, "Commande réservée à l'admin.", 30, 0);
            }
            break;
        
        default:
            char broadcast_msg[1024];
            snprintf(broadcast_msg, sizeof(broadcast_msg), "Message de %d : %s", sock, msg);
            sendAllClients(broadcast_msg);
            break;
    }

}
