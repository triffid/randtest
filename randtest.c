#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>

const int max = 37;             // number of faces on die
const long count = 10000000000;  // number of rolls per thread
const int nthreads = 16;        // number of threads

typedef struct {
	// thread itself
	pthread_t thread;
	// input to thread
	unsigned  idx;
	uint64_t  seed;
	// output from thread
	uint64_t* hitcount;
	double  * mean;
} threadinfo;

void* threadfn(void* arg) {
	// get thread index so we know where to pull/push data
	threadinfo info = *((threadinfo*) arg);

	// localize working data to prevent cache thrashing
	uint64_t lseed = info.seed;
	uint64_t lhitcount[max] = {};
	double   lmean = 0;

	for (unsigned long i = 0; i < count; i++) {
		// fetch random from PRNG
		uint32_t rand = ((lseed >> 32) * max) >> 32;
		// PRNG advance
		lseed += (lseed * lseed) | 5u;

		// tally hit on received value
		lhitcount[rand]++;

		// update mean
		lmean += (rand * 1.0) / count;

		// print progress occasionally
		if ((i & ((1UL << 26) - 1)) == 0)
			fprintf(stderr, "\r[ %2d ] %lu of %lu (%lu%%)\r", info.idx, i, count, i * 100 / count);
	}

	// marshall back to main thread, cache thrashing doesn't matter now
	memcpy(info.hitcount, lhitcount, sizeof(lhitcount));
	*info.mean = lmean;

	return (void*) 0;
}

int main(int argc, char** argv) {
	uint64_t seed[nthreads];
	uint64_t hitcount[nthreads][max];
	double   mean[nthreads];

	int r = open("/dev/urandom", O_RDONLY);
	if (r >= 0) {
		read(r, seed, sizeof(seed));
		close(r);
	}
	else {
		return 1;
	}

	threadinfo threads[nthreads];

	{
		printf("Using %d threads\n", nthreads);
		pthread_attr_t attr;
		if (pthread_attr_init(&attr) != 0)
			return 1;
		for (int i = 0; i < nthreads; i++) {
			threads[i].idx = i;
			threads[i].seed = seed[i];
			threads[i].hitcount = hitcount[i];
			threads[i].mean = &mean[i];
			if (pthread_create(&threads[i].thread, &attr, threadfn, &threads[i]) != 0)
				return 1;
		}
		pthread_attr_destroy(&attr);
	}

	double allmean = 0;
	uint64_t allhitcount[max] = {};

	for (int i = 0; i < nthreads; i++) {
		void* r;
		pthread_join(threads[i].thread, &r);
		allmean += mean[i] / nthreads;
		for (int j = 0; j < max; j++)
			allhitcount[j] += hitcount[i][j];
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
