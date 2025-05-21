#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "Channel.h"

// Ouverture des fichiers
FileHandler open_file(const char *filename, const char *mode) {
    FileHandler file;
    file.fp = fopen(filename, mode);
    file.filename = filename;
    file.mode = mode;

    if(file.fp == NULL) {
        perror("Erreur pendant l'ouverture du fichier");
        exit(EXIT_FAILURE);
    }
    return file;
}

// Fermeture des fichiers
void close_file(FileHandler *file) {
    if (file->fp != NULL) { // on vérifie si le pointeur fp n'est pas nul (le fichier est déjà ouvert)
        fclose(file->fp);
        file->fp = NULL;
    }
}

// Ecriture dans le fichier
int write_file(FileHandler *file, const char *line) {
    if (file->fp == NULL) { // si le fichier est fermé
        return -1;
    }
    return fprintf(file->fp, "%s\n", line);
}

// Lecture du fichier
int read_file(FileHandler *file, char *buffer, size_t size) {
    if (file->fp == NULL) { // si le fichier est fermé
        return -1;
    }
    if (fgets(buffer, size, file->fp) == NULL) {
        return 0;
    }
    buffer[strcspn(buffer, "\n")] = '\0'; // retirer le '\n'
    return 1; // ligne lue avec succès
}

// Vérifier si un utilisateur est déjà enregistré 
int user_exists(FileHandler *file, const char *pseudo) {
    if (file->fp != NULL) {
        rewind(file->fp);
    }
    char buffer[500];
    while (read_file(file, buffer, sizeof(buffer))) { // Tant qu'une ligne est lue
        char saved_pseudo[100];
        char mdp[100];
        if (sscanf(buffer, "%s %s", saved_pseudo, mdp) == 2) {
            if (strcmp(saved_pseudo, pseudo) == 0) { // si pseudo égaux
                return 1; // trouvé
            }
        }
    }
    return 0; // non trouvé
}

// Sauvegarder un nouvel utilisateur
void save_user(const char *pseudo, const char *mdp) {
    FileHandler fileCheck = open_file("save_users.txt", "r");
    int exists = user_exists(&fileCheck, pseudo);
    close_file(&fileCheck);
    
    if (exists) return; // Déjà enregistré

    FileHandler fileWrite = open_file("save_users.txt", "a");
    char line[200];
    snprintf(line, sizeof(line), "%s %s", pseudo, mdp);
    write_file(&fileWrite, line);
    close_file(&fileWrite);
}

// Sauvegarde tous les salons dans un fichier
bool save_channels(const char *filename) {
    FileHandler file = open_file(filename, "w");
    
    if (file.fp == NULL) {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier pour sauvegarder les salons\n");
        return false;
    }
    
    lockChannelList();
    Channel *current = channelList.first;
    
    // Parcourir tous les salons sauf Hub (qui est toujours créé au démarrage)
    while (current != NULL) {
        if (strcmp(current->name, "Hub") != 0) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s %d", current->name, current->maxSize);
            write_file(&file, buffer);
        }
        current = current->next;
    }
    
    unlockChannelList();
    close_file(&file);
    return true;
}

// Charge les salons depuis un fichier
bool load_channels(const char *filename) {
    FILE *test = fopen(filename, "r");
    if (test == NULL) {
        fprintf(stderr, "Le fichier des canaux n'existe pas encore, pas de canaux à charger\n");
        return false;
    }
    fclose(test);
    
    FileHandler file = open_file(filename, "r");
    
    if (file.fp == NULL) {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier pour charger les canaux\n");
        return false;
    }
    
    char buffer[256];
    while (read_file(&file, buffer, sizeof(buffer))) {
        char name[100];
        int maxSize;
        
        if (sscanf(buffer, "%s %d", name, &maxSize) == 2) {
            printf("Restauration du canal: %s (taille max: %d)\n", name, maxSize);
            Channel *newChannel = createChannel(name, maxSize, NULL);
            
            if (newChannel == NULL) {
                fprintf(stderr, "Erreur lors de la création du canal %s\n", name);
            }
        }
    }
    
    close_file(&file);
    return true;
}



