#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"


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
    if (fgets(buffer, size, file->fp)== NULL) {
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



