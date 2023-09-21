/**
 * Questo codice simula il gioco di pari e dispari tra due player e un giudice. 
 * Per convenzione il player1 sarà sempre pari, mentre il payer2 sarà sempre dispari. 
 * I due player butteranno un numero randomico tra 0 e 10 
 * 
 * Per poter avviare il programma bisogna eseguire il seguente comando di compilazione.
 * 
 *  "gcc pari-dispari.c -o sorgente"  
 * 
 * Per avviarlo bisogna passare il numero di partire da voler "valutare"
 *  "./sorgente 4"
 * 
 * Nota: si presuppone di aver scaricato un compilatore gcc
*/

#include "lib-misc.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>



typedef enum{
    true = 1,
    false = 0
}bool; 

typedef struct {
    unsigned int mossaP1;
    unsigned int mossaP2; 
    unsigned int numberMatch; //massimo
    sem_t sem[3]; //uno per player e uno per i giudici
    bool done; 
    unsigned int numero_vittoriaP1;
    unsigned int numero_vittoriaP2;

}shared_memory;

typedef struct{
    pthread_t tid; 
    unsigned int number_thread; //se è un giocatore o un giudice

    shared_memory *shared;  

}thread_memory;

void *player(void *arg){ //funzione da passare al pthread_create 
    thread_memory *memoria_locale = (thread_memory *)arg;
    printf("Inizio thread del player %d \n", memoria_locale->number_thread);
    while(1){
        sem_wait(&(memoria_locale->shared->sem[memoria_locale->number_thread])); 
        //se ho finito la partita
        if((memoria_locale->shared->done) == true){
            break; 
        }

        if(memoria_locale->number_thread == 0){
            
            memoria_locale->shared->mossaP1 = rand() % 11;
            printf("Player %d ha lanciato %d \n", memoria_locale->number_thread, memoria_locale->shared->mossaP1 );
        }else{
            memoria_locale->shared->mossaP2 = rand() % 11;
            printf("Player %d ha lanciato %d \n", memoria_locale->number_thread, memoria_locale->shared->mossaP2 );
        }
        

        sem_post(&memoria_locale->shared->sem[2]);
    }

    printf("Player %d chiude \n", memoria_locale->number_thread);
    pthread_exit(NULL); 
    
}

void *judge(void *arg){
    thread_memory *memoria_locale = (thread_memory *)arg;
    unsigned int numero_match_giocati = 0; 
    printf("Inizio thread del giudice %d \n", memoria_locale->number_thread);
    while(1){
        sem_wait(&memoria_locale->shared->sem[2]);
        sem_wait(&memoria_locale->shared->sem[2]); 

        if((memoria_locale->shared->done) == true){
            break; 
        }

        //definire il vincitore 
        unsigned int somma = (memoria_locale->shared->mossaP1) + (memoria_locale->shared->mossaP2);
        if((somma % 2) == 0){
            printf("Ha vinto il player 1 il match %d \n", numero_match_giocati);
            memoria_locale->shared->numero_vittoriaP1 ++;
        }else{
            printf("Ha vinto il player 2 il match %d \n", numero_match_giocati);
            memoria_locale->shared->numero_vittoriaP2 ++;
        }

        numero_match_giocati ++; 

        if(numero_match_giocati >= memoria_locale->shared->numberMatch){
            memoria_locale->shared->done = true; 
                    sem_post(&memoria_locale->shared->sem[0]);
        sem_post(&memoria_locale->shared->sem[1]);
            break;
        }
        
        sem_post(&memoria_locale->shared->sem[0]);
        sem_post(&memoria_locale->shared->sem[1]);
    }

    if(memoria_locale->shared->numero_vittoriaP1 > memoria_locale->shared->numero_vittoriaP2){
        printf("[Giocatore P1 ha vinto] \n");
    }else if(memoria_locale->shared->numero_vittoriaP1 < memoria_locale->shared->numero_vittoriaP2){
        printf("[Giocatore P2 ha vinto] \n");
    }else{
        printf("[PAREGGIO] \n");
    }
printf("Chiudo il giudice \n");
    pthread_exit(NULL); 
}


int main(int argc, char **argv){
    
    
    if(argc < 2 ){
        perror("Numero alto di argomenti \n");
        exit(1);
    }

    unsigned int n_match = atoi(argv[1]);

    if(n_match == 0){
        perror("POCHI ARGOMENTI \n");
        exit(1);
    }

    //memoria da inizializzare 
    shared_memory *memoria_condivisa = malloc(sizeof(shared_memory));
    thread_memory thread_memory_list[3]; 

    memoria_condivisa->done = false; 
    memoria_condivisa->numberMatch = n_match; 
    memoria_condivisa->numero_vittoriaP1 = 0;
    memoria_condivisa->numero_vittoriaP2 = 0;
     
    sem_init(&memoria_condivisa->sem[0], 0,1);
    sem_init(&memoria_condivisa->sem[1], 0,1);
    sem_init(&memoria_condivisa->sem[2], 0,0);

    for(int i = 0; i< 3 ; i++){
        thread_memory_list[i].number_thread = i; 
        thread_memory_list[i].shared = memoria_condivisa; 
    }


    //creiamo i thread 
    pthread_create(&thread_memory_list[0].tid, NULL, player, (void *)&thread_memory_list[0] );
    pthread_create(&thread_memory_list[1].tid, NULL, player, (void *)&thread_memory_list[1] );
    pthread_create(&thread_memory_list[2].tid, NULL, judge, (void *)&thread_memory_list[2] );

    /*for(int i = 0; i<3; i++){
        printf("Ciao \n");
        if(pthread_join(&thread_memory_list[i].tid, NULL) !=0){
            printf("Errore join\n");
            exit(1);
        }
        

    }*/

    pthread_join(thread_memory_list[0].tid, NULL);
    printf("Chiuso 0 \n"); 
    pthread_join(thread_memory_list[1].tid, NULL);
    printf("Chiuso 1 \n"); 
    pthread_join(thread_memory_list[2].tid, NULL);
    printf("Chiuso 2 \n"); 


    printf("Fine partita \n"); 
    exit(0); 
}