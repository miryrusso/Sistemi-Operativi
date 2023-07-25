/**
 * In questo codice la chiusura viene gestita dalla barriera
*/
#define _GNU_SOURCE
#include "lib-misc.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define BUFFER_SIZE 1024

typedef struct
{
    char buffer[BUFFER_SIZE];
    bool done; // iesimo reader ha completato il lavoro
    unsigned turn;

    unsigned nreader;      // numero di reader
    pthread_mutex_t lock;  // lock
    pthread_cond_t *pcond; // puntatore pthread perchè non sappiamo a priori quante variabili condizioni ci serviranno
} shared_rf;

void init_shared_rf(shared_rf *s, unsigned nreader)
{
    int err;

    s->done = 0;
    s->turn = 1;          // Rappresenta il primo che si deve sbloccare
    s->nreader = nreader; // numero di reader

    if ((err = pthread_mutex_init(&s->lock, NULL)) != 0)
        exit_with_err("pthread_mutex_init", err);

    s->pcond = malloc(sizeof(pthread_cond_t) * (nreader + 1)); // malloc per il lock

    for (unsigned i = 0; i < nreader + 1; i++) // inizializziamo ogni variabile condizione
        if ((err = pthread_cond_init(&s->pcond[i], NULL)) != 0)
            exit_with_err("pthread_cond_init", err);
}

void destroy_shared_rf(shared_rf *s)
{
    pthread_mutex_destroy(&s->lock);

    for (unsigned i = 0; i < s->nreader + 1; i++)
        pthread_cond_destroy(&s->pcond[i]);

    free(s->pcond);
    free(s);
}

// seconda struttura dati che ci permette di creare una comunicazione tra filtro e scrittore
typedef struct
{
    char buffer[BUFFER_SIZE];
    bool turn; // sarà 0 o 1 visto che abbiamo solo F e W
    bool done;

    pthread_mutex_t lock;
    pthread_cond_t pcond[2];
    pthread_barrier_t barrier; // gestire la chiusura dei thread con la barriera e non con pthread_join
} shared_fw;

void init_shared_fw(shared_fw *s)
{
    int err;

    s->turn = s->done = 0;

    if ((err = pthread_mutex_init(&s->lock, NULL)) != 0)
        exit_with_err("pthread_mutex_init", err);

    for (int i = 0; i < 2; i++)
        if ((err = pthread_cond_init(&s->pcond[i], NULL)) != 0)
            exit_with_err("pthread_cond_init", err);

    //barriere
    if ((err = pthread_barrier_init(&s->barrier, NULL, 3)) != 0) // filtro scrittore e scrittore
        exit_with_err("pthread_barrier_init", err);
}

void destroy_shared_fw(shared_fw *s)
{
    pthread_mutex_destroy(&s->lock);

    for (int i = 0; i < 2; i++)
        pthread_cond_destroy(&s->pcond[i]);

    pthread_barrier_destroy(&s->barrier);

    free(s);
}

typedef struct
{
    // dati privati
    pthread_t tid;
    unsigned thread_n; // per identificare un thread
    char *filename;    // passare argv[i]
    char *word;        // la parola su cui operare
    bool i_flag;       // flag
    bool v_flag;       // flag

    // dati condivisi
    shared_rf *srf;
    shared_fw *sfw;
} thread_data;

bool reader_put_line(thread_data *td, char *strt)
{
    int err;
    char *line;
    bool ret_value = 1;
    //cerca di ottenere il lock e SOLO DOPO usiamo la variabile condizione. 
    if ((err = pthread_mutex_lock(&td->srf->lock)) != 0)
        exit_with_err("pthread_mutex_lock", err);

    while (td->srf->turn != td->thread_n)
        if ((err = pthread_cond_wait(&td->srf->pcond[td->thread_n],
                                     &td->srf->lock)) != 0) 
            exit_with_err("pthread_cond_wait", err);

    //la invochiamo dopo che la condizione venga soddisfatta cosi che un solo thread alla volta possa effettuare questa operazione
    if ((line = strtok(strt, "\n")) == NULL)
    {
        ret_value = 0;
        td->srf->done = 1;
    }
    else
        strncpy(td->srf->buffer, line, BUFFER_SIZE);

    td->srf->turn = 0; //imposto il turno a 0 per dare il via al filtro 

    if ((err = pthread_cond_signal(&td->srf->pcond[0])) != 0)
        exit_with_err("pthread_cond_signal", err);

    if ((err = pthread_mutex_unlock(&td->srf->lock)) != 0)
        exit_with_err("pthread_mutex_unlock", err);

    return ret_value;
}

void reader(void *arg)
{
    thread_data *td = (thread_data *)arg;
    int err, fd;
    char *map, *strt;
    struct stat statbuf; // ci serve per fare struct stat

    if ((fd = open(td->filename, O_RDONLY)) == -1)
        exit_with_sys_err("open");

    if (fstat(fd, &statbuf) == -1) // ci serve la dimensione per fare la mappatura in memoria
        exit_with_sys_err("fstat");

    if ((map = mmap(NULL, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                    fd, 0)) == NULL) // modalità di mappatura gli passiamo sia read che write perchè utilizzeremo strtok,
        // per poter funzionare mette un \0 alla fine di conseguenza deve avere modo di poter manipolare il file
        // ricordiamo che le modifiche che facciamo nnon devono finire sul disco e quindi ci serve il permesso write
        //(MAP_PRIVATE)
        exit_with_sys_err("mmap");

    /*Leggere un file riga per riga con la mappatura?
        - Open File
        - FStat
        - Mappatura bisogna aggiungere  PROT_WRITE per poter manipolare il file
    */

    if (close(fd) == -1)
        exit_with_sys_err("close");

    reader_put_line(td, map); // leggere il file riga per riga

    while (reader_put_line(td, NULL)) //fin quando è finito il file 
        ;

    munmap(map, statbuf.st_size);
    pthread_exit(NULL);
}

bool filter_pass(thread_data *td, char *line)
{
    char *word = NULL;

    if (td->i_flag)
        word = strcasestr(line, td->word); //uguale ma è case insensitive
    else
        word = strstr(line, td->word); //ci restituisce un puntatore ad una word, cerchiamo la stringa nel buffer

    if (td->v_flag)
        return word == NULL; 
    else
        return word != NULL; //restituisco 1 quando ho trovato la parola.
}

void filterer(void *arg)
{
    int err;
    thread_data *td = (thread_data *)arg;
    unsigned actual_reader = 1; //perchè di defauld anche il filtro è 1
    char *line = NULL;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        if ((err = pthread_mutex_lock(&td->srf->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        while (td->srf->turn != 0)
            if ((err = pthread_cond_wait(&td->srf->pcond[0], &td->srf->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        if (td->srf->done) //impostato dal reader
        {
            actual_reader++;

            if (actual_reader > td->srf->nreader)
                break;

            td->srf->done = 0;
            td->srf->turn = actual_reader;

            if ((err = pthread_cond_signal(&td->srf->pcond[actual_reader])) !=
                0)
                exit_with_err("pthread_cond_signal", err);

            if ((err = pthread_mutex_unlock(&td->srf->lock)) != 0)
                exit_with_err("pthread_mutex_unlock", err);

            continue;
        }
        else //se il done fosse settato a 1 leggiamo dal buffer ciò che c'è stato scritto 
            strncpy(buffer, td->srf->buffer, BUFFER_SIZE);

        td->srf->turn = actual_reader;

        if ((err = pthread_cond_signal(&td->srf->pcond[actual_reader])) != 0)
            exit_with_err("pthread_cond_signal", err);

        if ((err = pthread_mutex_unlock(&td->srf->lock)) != 0)
            exit_with_err("pthread_mutex_unlock", err);

        if (filter_pass(td, buffer))
        {
            // Inserimento nella struttura dati condivisa con il Writer
            if ((err = pthread_mutex_lock(&td->sfw->lock)) != 0) //invia al writer la stringa 
                exit_with_err("pthread_mutex_lock", err);

            while (td->sfw->turn != 0)
                if ((err = pthread_cond_wait(&td->sfw->pcond[0],
                                             &td->sfw->lock)) != 0)
                    exit_with_err("pthread_cond_wait", err);

            strncpy(td->sfw->buffer, buffer, BUFFER_SIZE);
            td->sfw->turn = 1;

            if ((err = pthread_cond_signal(&td->sfw->pcond[1])) != 0)
                exit_with_err("pthread_cond_signal", err);

            if ((err = pthread_mutex_unlock(&td->sfw->lock)) != 0)
                exit_with_err("pthread_mutex_unlock", err);
        }
    }

    if ((err = pthread_mutex_lock(&td->sfw->lock)) != 0)
        exit_with_err("pthread_mutex_lock", err);

    while (td->sfw->turn != 0)
        if ((err = pthread_cond_wait(&td->sfw->pcond[0], &td->sfw->lock)) != 0)
            exit_with_err("pthread_cond_wait", err);

    td->sfw->done = 1;
    td->sfw->turn = 1;

    if ((err = pthread_cond_signal(&td->sfw->pcond[1])) != 0)
        exit_with_err("pthread_cond_signal", err);

    if ((err = pthread_mutex_unlock(&td->sfw->lock)) != 0)
        exit_with_err("pthread_mutex_unlock", err);

    //aspetta quando ci arrivano tutti 
    if ((err = pthread_barrier_wait(&td->sfw->barrier)) > 0)
        exit_with_err("pthread_barrier_wait", err);

    pthread_exit(NULL);
}

void writer(void *arg)
{
    int err;
    thread_data *td = (thread_data *)arg;

    while (1)
    {
        if ((err = pthread_mutex_lock(&td->sfw->lock)) != 0)
            exit_with_err("pthread_mutex_lock", err);

        while (td->sfw->turn != 1)
            if ((err = pthread_cond_wait(&td->sfw->pcond[1], &td->sfw->lock)) !=
                0)
                exit_with_err("pthread_cond_wait", err);

        if (td->sfw->done)
            break;

        printf("%s\n", td->sfw->buffer);
        td->sfw->turn = 0;

        if ((err = pthread_cond_signal(&td->sfw->pcond[0])) != 0)
            exit_with_err("pthread_cond_signal", err);

        if ((err = pthread_mutex_unlock(&td->sfw->lock)) != 0)
            exit_with_err("pthread_mutex_unlock", err);
    }

    if ((err = pthread_barrier_wait(&td->sfw->barrier)) > 0)
        exit_with_err("pthread_barrier_wait", err);

    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s [-v] [-i] [word] <file-1> [file-2] [file-3] [...]\n",
               argv[0]);

        exit(EXIT_FAILURE);
    }

    int err;
    int _from = 1; // indica da quale indice dobbiamo iniziare a leggere per fare il parsing dello step successivo
    char *word;
    bool v_flag = 0;
    bool i_flag = 0;

    // parsing delle flag -i/-v (opzionali).
    // verifico che argv[1] è = -v
    if (!strcmp(argv[1], "-v") || !strcmp(argv[2], "-v"))
    {
        v_flag = 1;
        _from++;
    }

    if (!strcmp(argv[1], "-i") || !strcmp(argv[2], "-i"))
    {
        i_flag = 1;
        _from++;
    }

    thread_data td[argc - _from + 1]; // creare un array che contiene i dati per i nostri thread <argc - _from + 1>
    // dove from sono i campi opzionali e 1  la parola cosi i thread corrispondono ai file passati.
    shared_rf *srf = malloc(sizeof(shared_rf));
    shared_fw *sfw = malloc(sizeof(shared_fw));
    init_shared_rf(srf, argc - _from - 1);
    init_shared_fw(sfw);

    unsigned thread_data_index = 0; // rappresenta un indice di inserimento

    // Reader
    for (unsigned i = _from + 1; i < argc; i++)
    {
        td[thread_data_index].filename = argv[i];
        td[thread_data_index].srf = srf;
        td[thread_data_index].thread_n = thread_data_index + 1; // i reader avranno un indice a partire da 1

        if ((err = pthread_create(&td[thread_data_index].tid, NULL,
                                  (void *)reader, &td[thread_data_index])) != 0)
            exit_with_err("pthread_create", err);

        thread_data_index++;
    }

    // Filterer
    td[thread_data_index].i_flag = i_flag;
    td[thread_data_index].v_flag = v_flag;
    td[thread_data_index].sfw = sfw;
    td[thread_data_index].srf = srf;
    td[thread_data_index].thread_n = 0;
    td[thread_data_index].word = argv[_from];

    if ((err = pthread_create(&td[thread_data_index].tid, NULL,
                              (void *)filterer, &td[thread_data_index])) != 0)
        exit_with_err("pthread_create", err);

    // Writer
    thread_data_index++;
    td[thread_data_index].sfw = sfw;

    if ((err = pthread_create(&td[thread_data_index].tid, NULL, (void *)writer,
                              &td[thread_data_index])) != 0)
        exit_with_err("pthread_create", err);

    // riguarda le barriere

    /*for (unsigned i = 0; i < thread_data_index + 1; i++)
        if ((err = pthread_detach(td[i].tid)) != 0)
            exit_with_err("pthread_detach", err);*/

    if ((err = pthread_barrier_wait(&sfw->barrier)) > 0)
        exit_with_err("pthread_barrier_wait", err);

    destroy_shared_rf(srf);
    destroy_shared_fw(sfw);
}