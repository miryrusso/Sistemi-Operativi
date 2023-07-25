#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include"../Lezioni/lib-misc.h"
#include <semaphore.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>
#include <time.h>


typedef enum{PLAYER1, PLAYER2, GIUDICE, TAB}thread_n; 
typedef enum{CARTA=0, FORBICE, SASSO}mossa; 

typedef struct{
    mossa mossa[2]; 
    bool done; 
    int winner; 

    sem_t sem[4]; 
}shared; 


typedef struct{
    pthread_t pid; 
    unsigned id;
    int numero_partite; 


    shared *sh; 
}thread_data; 


void init_shared(shared *sh){
    int err; 

    sh->done = 0; 

    if((err = sem_init(&sh->sem[PLAYER1], 0,1)) != 0){
        exit_with_err("sem_init", err);
    }

    if((err = sem_init(&sh->sem[PLAYER2], 0,1)) != 0){
        exit_with_err("sem_init", err);
    }

    if((err = sem_init(&sh->sem[GIUDICE], 0,0)) != 0){
        exit_with_err("sem_init", err);
    }

    if((err = sem_init(&sh->sem[TAB], 0,0)) != 0){
        exit_with_err("sem_init", err);
    }
    
}

void destroy(shared *sh){
    for(int i=0; i<4; i++){
        sem_destroy(&sh->sem[i]);
    }

    free(sh);
}

void player(void *arg){
    thread_data *td = (thread_data *)arg; 
    int err; 
    char mosse_txt[3][10];
    strncpy(mosse_txt[0], "CARTA",6);
    strncpy(mosse_txt[1], "FORBICE",8);
    strncpy(mosse_txt[2], "SASSO",6);

    while(1){
        if((err = sem_wait(&td->sh->sem[td->id])) != 0){
            exit_with_err("sem_wait", err); 
        }

        if(td->sh->done){
            break; 
        }

        td->sh->mossa[td->id] = rand() % 3; 
        printf("[PLAYER %d] ha lanciato: %s \n", td->id +1, mosse_txt[td->sh->mossa[td->id]] );


        //signal sul Giudice per far capire che ho inviato la mia mossa 
        if((err = sem_post(&td->sh->sem[GIUDICE])) != 0){
            exit_with_err("sem_post", err); 
        }
    }

    pthread_exit(NULL); 
}

int whowin(mossa *mosse){
    if(mosse[0] == mosse[1]){
        return 0; 
    }

    if((mosse[0] == CARTA && mosse[1] == SASSO)
        || (mosse[0]== FORBICE && mosse[1] == CARTA)
        || (mosse[0]== SASSO && mosse[1] == FORBICE)){
            return 1; 
        }
    
    return 2; 
}

void judge(void *arg){
    thread_data *td = (thread_data *)arg; 
    int err; 
    char winner; 

    while(1){
        //attendo il primo giocatore
        if((err = sem_wait(&td->sh->sem[GIUDICE])) != 0){
            exit_with_err("sem_wait_giudice", err); 
        }

        //attendo il secondo giocatore
        if((err = sem_wait(&td->sh->sem[GIUDICE])) != 0){
            exit_with_err("sem_wait_giudice", err); 
        }

        if(td->sh->done){
            break; 
        }

        //valuto chi vince 
        winner = whowin(td->sh->mossa);

        if(winner == 0){
            printf("[JUDGE] Pareggio \n");
            if((err = sem_post(&td->sh->sem[PLAYER1])) != 0){
                exit_with_err("sem_post", err); 
            }

            if((err = sem_post(&td->sh->sem[PLAYER2])) != 0){
                exit_with_err("sem_post", err); 
            }
        }else{
            td->sh->winner = winner; 

            if((err = sem_post(&td->sh->sem[TAB])) != 0){
                exit_with_err("sem_post tab", err); 
            }
        }

    }

    pthread_exit(NULL); 

}

/*Il processo T, in caso di vittoria, aggiornerà la propria classifica interna e avvierà, 
se necessario, una nuova partita. Sempre T, alla fine della serie di partite, decreterà 
l'eventuale vincitore.*/

void tab(void *arg){
    thread_data *td = (thread_data *)arg; 
    int err; 
    int score[2] = {0,0};  

    for(int i = 0 ; i<td->numero_partite; i++)
    {
        if((err = sem_wait(&td->sh->sem[TAB])) != 0){
                    exit_with_err("sem_post", err); 
        }

        score[td->sh->winner -1] ++; 

        

        if(i< td->numero_partite-1){
            printf("Ecco la classifica provvisoria: %d, %d \n", score[0], score[1]);

            if((err = sem_post(&td->sh->sem[PLAYER1])) != 0){
                exit_with_err("sem_post", err); 
            }

            if((err = sem_post(&td->sh->sem[PLAYER2])) != 0){
                exit_with_err("sem_post", err); 
            }

        }

    }

    printf("Ecco la classifica finale %d, %d \n", score[0], score[1]); 
    if(score[0] == score[1]){
        printf("La partita è finita in pareggio \n");
    }

    if(score[0] < score[1]){
        printf("Ha vinto il Player2 \n");
    }else if(score[0] > score[1]){
        printf("Ha vinto il Player1 \n");
    }

    /*if((err = sem_wait(&td->sh->sem[TAB])) != 0){
        exit_with_err("tab", err);
    }*/

    td->sh->done = 1; 

    if((err = sem_post(&td->sh->sem[PLAYER1])) != 0){
        exit_with_err("sem_post", err); 
    }

    if((err = sem_post(&td->sh->sem[PLAYER2])) != 0){
        exit_with_err("sem_post", err); 
    }


    if((err = sem_post(&td->sh->sem[GIUDICE])) != 0){
        exit_with_err("sem_post", err); 
    }


    if((err = sem_post(&td->sh->sem[GIUDICE])) != 0){
        exit_with_err("sem_post", err); 
    }

    pthread_exit(NULL); 
}

int main(int argc, char **argv){
    thread_data td[4]; 
    shared *sh = malloc(sizeof(shared));
    init_shared(sh); 
    int err;

    srand(time(NULL)); 
    
    if(argc != 2 &&  atoi(argv[1]) == 0){
        printf("Usage %s <numero_partite> \n Il numero delle partite deve essere maggiore di 0 \n", argv[0]);
        exit(1);
    }

    int numero_partite = atoi(argv[1]); 

    //inizializzare la struttra thread_data 
    for(int i = 0; i<4; i++){
        td[i].id = i; 
        td[i].sh = sh;
        td[i].numero_partite = numero_partite;
    }


    //creo i vari thread
    if((err = pthread_create(&td[PLAYER1].pid, NULL, (void *)player, &td[PLAYER1])) != 0){
        exit_with_err("create", err);
    }

    if((err = pthread_create(&td[PLAYER2].pid, NULL, (void *)player, &td[PLAYER2])) != 0){
        exit_with_err("create", err);
    }

    if((err = pthread_create(&td[GIUDICE].pid, NULL, (void *)judge, &td[GIUDICE])) != 0){
        exit_with_err("create", err);
    }

    if((err = pthread_create(&td[TAB].pid, NULL, (void *)tab, &td[TAB])) != 0){
        exit_with_err("create", err);
    }

    //pthread join 
    for(int i =  0; i<4; i++){
        if((err = pthread_join(td[i].pid, NULL)) != 0){
            exit_with_err("create", err);
        }
    }


    destroy(sh); 
    exit(EXIT_SUCCESS); 
}