#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

typedef struct {
    sem_t mutex;
} Monitor;


typedef struct {
    sem_t sem;
    int   count;
} Condition;

typedef struct {
    Monitor monitor;
    Condition cond_prod;
    Condition cond_cons;
    int buffer;
    int full;
} SharedBuffer;


void monitor_init(Monitor *m) {
    sem_init(&m->mutex, 0, 1);
}

void monitor_enter(Monitor *m) {
    sem_wait(&m->mutex);
}
void monitor_leave(Monitor *m) {
    sem_post(&m->mutex);
}
void monitor_wait(Condition *c, Monitor *m) {
    c->count++;
    monitor_leave(m);
    sem_wait(&c->sem);
    monitor_enter(m);
}

void monitor_signal(Condition *c) {
    if (c->count > 0) {
        c->count--;
        sem_post(&c->sem);
    }
}

void condition_init(Condition *c) {
    sem_init(&c->sem, 0, 0);
    c->count = 0;
}
void condition_destroy(Condition *c) {
    sem_destroy(&c->sem);
}

void shared_buffer_init(SharedBuffer *shared) {
    monitor_init(&shared->monitor);
    condition_init(&shared->cond_prod);
    condition_init(&shared->cond_cons);
    shared->buffer = 0;
    shared->full = 0;
}

void shared_buffer_destroy(SharedBuffer *shared) {
    condition_destroy(&shared->cond_prod);
    condition_destroy(&shared->cond_cons);
    sem_destroy(&shared->monitor.mutex);
}

void *producer(void *arg) {
    SharedBuffer *shared = (SharedBuffer *)arg;

    for (int value = 1; value <= 5; value++) {
        monitor_enter(&shared->monitor);
        while (shared->full) {
            monitor_wait(&shared->cond_prod, &shared->monitor);
        }

        shared->buffer = value;
        shared->full = 1;
        printf("Producer wrote: %d\n", shared->buffer);

        monitor_signal(&shared->cond_cons);
        monitor_leave(&shared->monitor);
    }

    return NULL;
}

void *consumer(void *arg) {
    SharedBuffer *shared = (SharedBuffer *)arg;

    for (int i = 0; i < 5; i++) {
        int value;

        monitor_enter(&shared->monitor);
        while (!shared->full) {
            monitor_wait(&shared->cond_cons, &shared->monitor);
        }

        value = shared->buffer;
        shared->full = 0;
        printf("Consumer read: %d\n", value);

        monitor_signal(&shared->cond_prod);
        monitor_leave(&shared->monitor);
    }

    return NULL;
}

int main(void) {
    SharedBuffer shared;
    pthread_t producer_thread;
    pthread_t consumer_thread;

    shared_buffer_init(&shared);

    pthread_create(&producer_thread, NULL, producer, &shared);
    pthread_create(&consumer_thread, NULL, consumer, &shared);

    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    shared_buffer_destroy(&shared);

    return 0;
}

