#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Lezioni/lib-misc.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>


#define MAX_BUFFER 4096

typedef enum{R_THREAD, P_THREAD, W_THREAD}thread_n; 

typedef struct {
    char buffer[MAX_BUFFER];
    bool done; 

    sem_t sem[3];
}shared;

typedef struct{
    pthread_t tid; 
    unsigned thread_n; 
    char *filename; 

    shared *sh; 

}thread_data;

void init_shared(shared *sh){
    int err; 

    sh->done = 0; 
    
    //inizializziamo i semafori
    if((err = sem_init(&sh->sem[R_THREAD],0,1)) != 0)
        exit_with_err("sem_init R", err);
    
    if((err = sem_init(&sh->sem[W_THREAD],0,0)) != 0)
        exit_with_err("sem_init R", err);
    
    if((err = sem_init(&sh->sem[P_THREAD],0,0)) != 0)
        exit_with_err("sem_init R", err);

}

void destroy(shared *sh){
    for(int i = 0 ; i< 3; i++){
        sem_destroy(&sh->sem[i]);
    }

    free(sh); 
}

/*il thread R leggerà il file riga per riga e inserirà, ad ogni iterazione, 
la riga letta (parola) all'interno della struttura dati condivisa;
*/

void reader(void *arg){
    thread_data *td = (thread_data *)arg; 
    //cosa deve fare questa funzione
    char buffer[MAX_BUFFER]; //per leggere 
    int err; 

    FILE *f;  
    printf("Ho aperto il file \n");
    if((f = fopen(td->filename, "r")) == NULL){
        exit_with_sys_err("fopen");
    }

    printf("HSto leggendo il file \n");
    while(fgets(buffer, MAX_BUFFER, f)){

        if(buffer[strlen(buffer) -1] == '\n'){
            buffer[strlen(buffer) -1] = '\0';
        }

        if((err = sem_wait(&td->sh->sem[R_THREAD])) != 0){
            exit_with_err("sem_wait", err);
        }
        
        printf("Parola buffer \n");
        strncpy(td->sh->buffer, buffer, MAX_BUFFER);

        //fine sezione critica 
        if((err = sem_post(&td->sh->sem[P_THREAD])) != 0){
            exit_with_err("sem_post", err);
        }

        memset(buffer, '\0', MAX_BUFFER);

    }


    printf("Lettura completata perchè ho finito il file \n");
    //dobbiamo dire che il file e la lettura è stata completata 
    if((err = sem_wait(&td->sh->sem[R_THREAD])) != 0){
        exit_with_err_msg("sem_wait");
    }

    td->sh->done = 1; 

    printf("Done = 1 \n");

    if((err = sem_post(&td->sh->sem[P_THREAD])) != 0){
        exit_with_err("sem_post", err); 
    }

    if((err = sem_post(&td->sh->sem[W_THREAD])) != 0){
        exit_with_err("sem_post", err); 
    }


    printf("Chiudo file \n");

    fclose(f); 
}

bool is_palindrome(char *s) {
    for (int i = 0; i < (int) strlen(s)/2; i++)
        if (s[i] != s[strlen(s) - 1 - i])
            return 0;

    return 1;
}
//funzione che controlla se una parola è palindroma


/*il thread P analizzerà, ad ogni iterazione, la parola inserita da R nella struttura dati; 
se la parola è palindroma, 
P dovrà passarla al thread W "svegliandolo";*/
void p_thread(void *arg){
    thread_data *td = (thread_data *)arg; 
    int err;

    printf("Ma qui ci sono?'\n");
    while(1){
        printf("[P] sono nel ciclo \n");
        if((err = sem_wait(&td->sh->sem[P_THREAD])) !=0 )
            exit_with_err("wait palindroma", err);
        

        printf("[P] controllo se è palindroma \n");
        
        if(is_palindrome(td->sh->buffer) != 0){
            if((err = sem_post(&td->sh->sem[W_THREAD])) != 0){
                exit_with_err("POST palindroma", err);
            }
        }else{
            if((err = sem_post(&td->sh->sem[R_THREAD])) != 0){
                exit_with_err("POST palindroma", err);
            }
        }

        if(td->sh->done){ break ; }


        
    }

}


//il thread W, ad ogni "segnalazione" di P, scriverà sullo standard output la parola palindroma.

void writer(void *arg){
    thread_data *td = (thread_data *)arg; 
    int err; 
    while(1){
        if((err = sem_wait(&td->sh->sem[W_THREAD])) != 0){
            exit_with_err("sem wait W", err); 
        }

        if(td->sh->done){
            break; 
        }

        printf("[WRITER] parola Palindroma: %s", td->sh->buffer);

        if((err = sem_post(&td->sh->sem[R_THREAD])) != 0){
            exit_with_err("sem wait P", err); 
        }

        
    }

}

int main(int argc, char **argv){
    thread_data td[3]; 
    shared *sh = malloc(sizeof(shared));
    init_shared(sh); 
    int err; 

    if(argc<2){
        printf("Usage %s <file.txt>", argv[0]);
        exit(1);
    }

    //inizializzare i thread 
    for(int i = 0; i<3; i++){
        td[i].sh = sh; 
    }
    
    td[R_THREAD].filename = argv[1];

    //devo creare i thread 
    if((err = pthread_create(&td[R_THREAD].tid, NULL, (void *)reader, &td[R_THREAD])) != 0){
        exit_with_err("pthread_create_R", err);
    }


    if((err = pthread_create(&td[W_THREAD].tid, NULL, (void *)writer, &td[W_THREAD])) != 0){
        exit_with_err("pthread_create_W", err);
    }


    if((err = pthread_create(&td[P_THREAD].tid, NULL, (void *)p_thread, &td[P_THREAD])) != 0){
        exit_with_err("pthread_create_P", err);
    }


    //aspetto che tutti finiscono
    for(int i = 0; i<3; i++){
        if((err = pthread_join(td[i].tid, NULL)) != 0){
            exit_with_err("pthread_JOIN", err);
        }
    }

    destroy(sh); 
    exit(EXIT_SUCCESS); 

}