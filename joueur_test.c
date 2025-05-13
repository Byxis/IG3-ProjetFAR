#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"  // Assure-toi que file.h et file.c sont bien compilés avec ce fichier

int main() {
    char pseudo[100];
    char mdp[100];

    printf("Nom du joueur : ");
    if (scanf("%99s", pseudo) != 1) {
        printf("Erreur de saisie pseudo.\n");
        return 1;
    }

    printf("Mot de passe : ");
    if (scanf("%99s", mdp) != 1) {
        printf("Erreur de saisie mot de passe.\n");
        return 1;
    }

    save_user(pseudo, mdp);

    printf("✅ Pseudo '%s' enregistré (sauf s'il l'était déjà).\n", pseudo);
    return 0;
}