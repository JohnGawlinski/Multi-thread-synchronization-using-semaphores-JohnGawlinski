#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

/* ---- random range ---- */
int rand_range(int a, int b)
{
	return a + rand() % (b - a + 1);
}

/* ---- utility: safe printing ---- */

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
void safe_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pthread_mutex_lock(&print_mutex);
	vprintf(fmt, ap);
	fflush(stdout);
	pthread_mutex_unlock(&print_mutex);
	va_end(ap);
}

/* ---- small sleep (milliseconds) ---- */

void msleep(unsigned int ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&ts, NULL);
}

/* ---- Problem 1: No-starve ---- */

sem_t P1_serviceQueue;
sem_t P1_resource;
pthread_mutex_t P1_readCount_mutex = PTHREAD_MUTEX_INITIALIZER;
int P1_readCount = 0;

void *P1_reader(void *arg)
{
	long id = (long)arg;
	while (1) {
		sem_wait(&P1_serviceQueue);

		pthread_mutex_lock(&P1_readCount_mutex);
		if (P1_readCount == 0) sem_wait(&P1_resource);
		P1_readCount++;
		pthread_mutex_unlock(&P1_readCount_mutex);

		sem_post(&P1_serviceQueue);

		safe_printf("Reader %ld: reading\n", id);
		msleep(rand_range(50, 150));

		pthread_mutex_lock(&P1_readCount_mutex);
		P1_readCount--;
		if (P1_readCount == 0) sem_post(&P1_resource);
		pthread_mutex_unlock(&P1_readCount_mutex);

		msleep(rand_range(20, 100));
	}
	return NULL;
}

void *P1_writer(void *arg)
{
	long id = (long)arg;
	while (1) {
		sem_wait(&P1_serviceQueue);
		sem_wait(&P1_resource);
		sem_post(&P1_serviceQueue);

		safe_printf("Writer %ld: writing\n", id);
		msleep(rand_range(80, 180));

		sem_post(&P1_resource);

		msleep(rand_range(30, 120));
	}
	return NULL;
}

void run_problem1(void)
{
	const int NUM_READERS = 5;
	const int NUM_WRITERS = 5;

	sem_init(&P1_serviceQueue, 0, 1);
	sem_init(&P1_resource, 0, 1);

	pthread_t readers[NUM_READERS];
	pthread_t writers[NUM_WRITERS];

	for (long i = 0; i < NUM_READERS; ++i)
		pthread_create(&readers[i], NULL, P1_reader, (void*)(i + 1));
	for (long i = 0; i < NUM_WRITERS; ++i)
		pthread_create(&writers[i], NULL, P1_writer, (void*)(i + 1));

	for(int i = 0; i < NUM_READERS; ++i)
		pthread_join(readers[i], NULL);
	for(int i = 0; i < NUM_WRITERS; ++i)
		pthread_join(writers[i], NULL);

	sem_destroy(&P1_serviceQueue);
	sem_destroy(&P1_resource);
}

/* ---- Probelm 2: Writer-priority readers-writers ---- */

sem_t P2_readTry;
sem_t P2_resource;
pthread_mutex_t P2_rcount_mutex = PTHREAD_MUTEX_INITIALIZER;
int P2_readCount = 0;
pthread_mutex_t P2_wcount_mutex = PTHREAD_MUTEX_INITIALIZER;
int P2_writeCount = 0;

void *P2_reader(void *arg)
{
	long id = (long)arg;
	while (1) {
		sem_wait(&P2_readTry);

		pthread_mutex_lock(&P2_rcount_mutex);
		if (P2_readCount == 0) sem_wait(&P2_resource);
		P2_readCount++;
		pthread_mutex_unlock(&P2_rcount_mutex);

		sem_post(&P2_readTry);

		safe_printf("Reader %ld: reading\n", id);
		msleep(rand_range(50, 150));

		pthread_mutex_lock(&P2_rcount_mutex);
		P2_readCount--;
		if (P2_readCount == 0)
			sem_post(&P2_resource);
		pthread_mutex_unlock(&P2_rcount_mutex);

		msleep(rand_range(20, 100));
	}
	return NULL;
}

void *P2_writer(void *arg)
{
	long id = (long)arg;
	while (1) {
		/* indicate writer waiting */
		pthread_mutex_lock(&P2_wcount_mutex);
		P2_writeCount++;
		if (P2_writeCount == 1) sem_wait(&P2_readTry); /* block readers */
		pthread_mutex_unlock(&P2_wcount_mutex);

		sem_wait(&P2_resource);

		safe_printf("Writer %ld: writing\n", id);
		msleep(rand_range(80, 180));

		sem_post(&P2_resource);

		pthread_mutex_lock(&P2_wcount_mutex);
		P2_writeCount--;
		if (P2_writeCount == 0) sem_post(&P2_readTry); /* allow readers */
		pthread_mutex_unlock(&P2_wcount_mutex);

		msleep(rand_range(30, 120));
	}
	return NULL;
}

void run_problem2(void)
{
	const int NUM_READERS = 5;
	const int NUM_WRITERS = 5;

	sem_init(&P2_readTry, 0, 1);
	sem_init(&P2_resource, 0, 1);
	P2_readCount = 0;
	P2_writeCount = 0;

	pthread_t readers[NUM_READERS];
	pthread_t writers[NUM_WRITERS];

	for(long i = 0; i < NUM_READERS; ++i)
		pthread_create(&readers[i], NULL, P2_reader, (void*)(i + 1));
	for(long i = 0; i < NUM_WRITERS; ++i)
		pthread_create(&writers[i], NULL, P2_writer, (void*)(i + 1));

	for(int i = 0; i < NUM_READERS; ++i)
		pthread_join(readers[i], NULL);
	for(int i = 0; i < NUM_WRITERS; ++i)
		pthread_join(writers[i], NULL);

	sem_destroy(&P2_readTry);
	sem_destroy(&P2_resource);
}

/* ---- Problem 3: Dining Philosophers #1 ---- */

#define P3_N 4
sem_t P3_forks[P3_N];
sem_t P3_room;

void *P3_philosopher(void *arg)
{
	long id = (long)arg;
	int left = id;
	int right = (id + 1) %P3_N;

	while (1) {
		safe_printf("Philosopher %ld: Thinking\n", id+1);
		msleep(rand_range(50, 150));

		sem_wait(&P3_room); /* enter room (at most N-1) */

		sem_wait(&P3_forks[left]);
		sem_wait(&P3_forks[right]);

		safe_printf("Philosopher %ld: Eating\n", id+1);
		msleep(rand_range(80, 180));

		sem_post(&P3_forks[right]);
		sem_post(&P3_forks[left]);

		sem_post(&P3_room); /* leave room */
	}
	return NULL;
}

void run_problem3(void)
{
	for (int i = 0; i < P3_N; ++i) sem_init(&P3_forks[i], 0, 1);
	sem_init(&P3_room, 0, P3_N - 1);

	pthread_t threads[P3_N];
	for (long i = 0; i < P3_N; ++i)
		pthread_create(&threads[i], NULL, P3_philosopher, (void*) i);

	for (int i = 0; i < P3_N; ++i) pthread_join(threads[i], NULL);

	for (int i = 0; i < P3_N; ++i) sem_destroy(&P3_forks[i]);
	sem_destroy(&P3_room);
}

/* ---- Problem 4: Dining Philosophers #2 ---- */

#define P4_N 5
sem_t P4_forks[P4_N];

void *P4_philosopher(void *arg)
{
	long id = (long)arg;
	int left = id;
	int right = (id + 1) % P4_N;
	int pickLeftFirst = (id % 2 == 0); /* even -> left first, odd -> right first */

	while (1) {
		safe_printf("Philosopher %ld: Thinking\n", id);
		msleep(rand_range(50, 150));

		if (pickLeftFirst) {
			sem_wait(&P4_forks[left]);
			sem_wait(&P4_forks[right]);
		} else {
			sem_wait(&P4_forks[right]);
			sem_wait(&P4_forks[left]);
		}

		safe_printf("Philosopher %ld: Eating\n", id);
		msleep(rand_range(80, 180));

		sem_post(&P4_forks[left]);
		sem_post(&P4_forks[right]);

	}
	return NULL;
}

void run_problem4(void)
{
	for (int i = 0; i < P4_N; ++i) sem_init(&P4_forks[i], 0, 1);

	pthread_t threads[P4_N];
	for (long i = 0; i < P4_N; ++i)
		pthread_create(&threads[i], NULL, P4_philosopher, (void*)i);

	for (int i = 0; i < P4_N; ++i) pthread_join(threads[i], NULL);

	for(int i = 0; i < P4_N; ++i) sem_destroy(&P4_forks[i]);
}

/* ---- main ---- */

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <problem#>\nWhere problem# is 1..4\n", argv[0]);
		return 1;
	}

	srand((unsigned)time(NULL) ^ (unsigned)getpid());

	int prob = atoi(argv[1]);
	switch (prob) {
		case 1:
			safe_printf("Running Problem 1: No-starve readers-writers (5 readers, 5 writers)\n");
			run_problem1();
			break;
		case 2:
			safe_printf("Running Problem 2: Writer-priority readers-writers (5 readers, 5 writers)\n");
                        run_problem2();
                        break;
		case 3:
			safe_printf("Running Problem 3: Dining philosophers #1 (waiter N-1)\n");
			run_problem3();
			break;
		case 4:
			safe_printf("Running Problem 4: Dining philosophers #2 (asymmetric pickup)/n");
			run_problem4();
			break;
		default:
			fprintf(stderr, "Unknown problem number: %d\n", prob);
			return 1;
	}

	return 0;
}
