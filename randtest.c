#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>

const int max = 37;
const long count = 10000000000;

const int nthreads = 16;

uint64_t* seed;
uint64_t* hitcount;
double*   mean;

void* threadfn(void* arg) {
	unsigned myidx = (unsigned long) arg;

	// localize working data to prevent cache thrashing
	uint64_t lseed = seed[myidx];
	uint64_t lhitcount[max] = {};
	double lmean = 0;

	for (unsigned long i = 0; i < count; i++) {
		uint32_t rand = ((lseed >> 32) * max) >> 32;

		lhitcount[rand]++;

		lmean += (rand * 1.0) / count;

		lseed += (lseed * lseed) | 5u;

		if ((i & ((1UL << 26) - 1)) == 0) {
			fprintf(stderr, "\r[ %2d ] %lu of %lu (%lu%%)\r", myidx, i, count, i * 100 / count);
		}
	}

	// marshall back to main thread, cache thrashing doesn't matter now
	memcpy(&hitcount[myidx * max], lhitcount, sizeof(lhitcount));
	mean[myidx] = lmean;

	return (void*) 0;
}

int main(int argc, char** argv) {
	seed = malloc(sizeof(seed[0]) * nthreads);
	hitcount = malloc(sizeof(hitcount[0]) * nthreads * max);
	mean = malloc(sizeof(mean[0]) * nthreads);

	if (seed == nullptr) return 1;
	if (hitcount == nullptr) return 1;
	if (mean == nullptr) return 1;

	int r = open("/dev/urandom", O_RDONLY);
	if (r >= 0) {
		read(r, seed, sizeof(seed[0]) * nthreads);
		close(r);
	}
	else {
		return 1;
	}

	pthread_t threads[nthreads];

	{
		printf("Using %d threads\n", nthreads);
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0)
			return 1;
		for (int i = 0; i < nthreads; i++) {
			unsigned long arg = i;
			if (pthread_create(&threads[i], &attr, threadfn, (void*) arg) != 0)
				return 1;
		}
		pthread_attr_destroy(&attr);
	}

	double allmean = 0;
	uint64_t allhitcount[max] = {};

	for (int i = 0; i < nthreads; i++) {
		void* r;
		pthread_join(threads[i], &r);
		allmean += mean[i] / nthreads;
		for (int j = 0; j < max; j++) {
			allhitcount[j] += hitcount[i * max + j];
		}
		//printf("Thread %d finished\n", i);
	}

	//fprintf(stderr, "\r%lu of %lu (%lu%%)          \r", count, count, 100UL);
	puts("\r                                      \r");

	printf("Expected count: %lu                        \n", count * nthreads / max);

	for (int i = 0; i < max; i++) {
		double permille = ((double) allhitcount[i]) / (count * nthreads / max) - 1.0;
		printf("[ %2d ] %lu (%+0.7f‰)%c", i, allhitcount[i], 1000.0 * permille, (((i & 3) == 3) || (i == (max - 1)))?'\n':'\t');
	}
	printf("mean: %f / %f (%+0.5f‰)\n", allmean, max * 0.5 - 0.5, 1000.0 * (allmean / (max * 0.5 - 0.5) - 1.0));
}
