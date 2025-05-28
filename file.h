#ifndef FILE_H
#define FILE_H

#include <stdio.h>
#include <stdbool.h>

typedef struct {
    FILE *fp; // représente un fichier ouvert
    const char *filename;
    const char *mode; // mode d'ouverture du fichier (r, w, a)
} FileHandler;

FileHandler open_file(const char *filename, const char *mode);
void close_file(FileHandler *file);
int write_file(FileHandler *file, const char *line);
int read_file(FileHandler *file, char *buffer, size_t size);
int user_exists(FileHandler *file, const char *pseudo);
void save_user(const char *pseudo, const char *mdp);
bool save_channels(const char *filename);
bool load_channels(const char *filename);

#endif