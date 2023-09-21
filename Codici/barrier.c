/**
 * In questo esempio, abbiamo creato 3 thread e li abbiamo fatti lavorare contemporaneamente. 
 * Ogni thread si ferma alla barriera (pthread_barrier_wait()) e attende che gli altri thread 
 * raggiungano lo stesso punto prima di poter continuare con la fase successiva. 
 * Alla fine, distruggiamo la barriera con pthread_barrier_destroy().
*/

#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 3

// Dichiarazione della barriera
pthread_barrier_t barrier;

void* thread_function(void* thread_id) {
    int tid = *(int*)thread_id;
    printf("Thread %d sta svolgendo il proprio lavoro.\n", tid);

    // Attendi qui la sincronizzazione degli altri thread
    pthread_barrier_wait(&barrier);

    printf("Thread %d sta proseguendo con la fase successiva.\n", tid);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Inizializzazione della barriera con il numero di thread da aspettare (NUM_THREADS)
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    // Creazione dei thread
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    // Attendere il completamento dei thread
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Distruzione della barriera
    pthread_barrier_destroy(&barrier);

    return 0;
}
