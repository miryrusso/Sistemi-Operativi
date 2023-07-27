/**
 * Questo programma testa le funzioni dei file al fine di copiare un file in un altro 
 * [AVVIO] -> ./copy-stream <file da copiare> <file dove copiare> 
*/
#include "lib-misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_BUFFER 1024
//copia il file sorgente in uno di destinazione

int main(int argc, char **argv){
    FILE *in, *out;
    int c; 
    char buffer[MAX_BUFFER];

    //passo da linea di comando i due file 
    if(argc != 3){
        printf("Utilizzo: %s <sorgente> <destinazione> \n", argv[0]);
    }

    printf("Creo una copia di %s a %s \n", argv[0], argv[1]);

    //per prima cosa apro lo stream in sola lettura 
    if((in = fopen(argv[1], "r"))== NULL){
        exit_with_sys_err(argv[1]);
    }

    
    //Apre e crea lo stream di destinazione in sola scrittura
    if((out = fopen(argv[2], "w")) == NULL){
        exit_with_sys_err(argv[2]);
    }

    /*
    //copia i dati dalla sorgente alla destinazione carattere per carattere partendo da una variabile char
    while((c = fgetc(in)) != EOF){
        fputc(c, out);
    }

    printf("Ho copiato il file %s all'interno del file %s \n", argv[1], argv[2]); 
    */

    //Sposta il cursore alla fine del file e ovviamente non copio nulla perchè ritorna NULL fgets
    fseek(in, 0, SEEK_END);

    //copia i dati dalla sorgente alla destinazione riga per riga partendo da un buffer(?)
    while((fgets(buffer, MAX_BUFFER, in)) != NULL){
        fputs(buffer, out); 
    }

    printf("Ho copiato il file %s all'interno del file %s \n", argv[1], argv[2]);

    //remove(argv[2]);  //eliminazione del file 

    rename("index.txt", "appunti.txt"); //rinomina un file 

    fclose(in);
    fclose(out);

    exit(EXIT_SUCCESS);     
}