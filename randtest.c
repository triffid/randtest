/*
 multi-threaded PRNG distribution tester
 Copyright (C) 2025  Michael Moon

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <time.h>

#include <math.h>

#include <pthread.h>

#include <sys/param.h>

const int max = 32;             // number of faces on dice
const long count = 1000000000;  // number of rolls per thread
const int nthreads = 16;        // number of threads

void siprefix(double value, double* display, char* prefix) {
	double gr = MIN(MAX((int) floor(log10(value) / 3), -10), 10);
	*display = value / powf(10, 3 * gr);
	*prefix = "qryzafpnum_kMGTPEZYRQ"[((int) gr) + 10];
}

inline double sq(double a) { return a*a; }

// PRNG
// NOTE: runs 23% slower than manual inline at -O0 and -O1, parity at -O2 and -O3
static inline uint32_t PRNG_fetch(uint32_t max, uint64_t* seed) __attribute__ ((always_inline));
static inline uint32_t PRNG_fetch(uint32_t max, uint64_t* seed) {
	*seed += (*seed * *seed) | 5u;
	return ((*seed >> 32u) * max) >> 32u;
	// https://en.wikipedia.org/wiki/Lehmer_random_number_generator
	// *seed = ((*seed * 48271) % 0x7FFFFFFF);
	// return (*seed & 0x7FFFFFFF) % max;
}

typedef struct {
	// thread itself
	pthread_t thread;
	// input to thread
	unsigned  idx;
	uint64_t  seed;
	// output from thread
	uint64_t* hitcount;
	double  * mean;
	double  * sd;
} threadinfo;

void* threadfn(void* arg) {
	// get thread index so we know where to pull/push data
	threadinfo info = *((threadinfo*) arg);

	// localize working data to prevent cache thrashing
	uint64_t lseed = info.seed;
	uint64_t lhitcount[max] = {};
	double   lmean = 0;
	double   lsd   = 0;

	for (unsigned long i = 0; i < count; i++) {
		uint32_t rand = PRNG_fetch(max, &lseed);
		// uint32_t rand = i % max;

		// tally hit on received value
		lhitcount[rand]++;

		// update mean and sd
		lmean += (rand * 1.0) / count;
		lsd   += sq(rand - max * 0.5 - 0.5) / count / nthreads;

		// print progress occasionally
		if ((i & ((1UL << 26) - 1)) == 0)
			fprintf(stderr, "\r[ %2d ] %lu of %lu (%lu%%)\r", info.idx, i, count, i * 100 / count);
	}

	// marshall back to main thread, cache thrashing doesn't matter now
	memcpy(info.hitcount, lhitcount, sizeof(lhitcount));
	*info.mean = lmean;
	*info.sd   = lsd;

	return (void*) 0;
}

int main(int argc, char** argv) {
	uint64_t seed[nthreads];
	uint64_t hitcount[nthreads][max];
	double   mean[nthreads];
	double   sd[nthreads];

	int r = open("/dev/urandom", O_RDONLY);
	if (r >= 0) {
		int i = read(r, seed, sizeof(seed));
		close(r);
	}
	else {
		return 1;
	}

	struct timespec starttime;
	clock_gettime(CLOCK_MONOTONIC, &starttime);

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
			threads[i].sd   = &sd[i];
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

	struct timespec endtime;
	clock_gettime(CLOCK_MONOTONIC, &endtime);

	printf("\rExpected count: %lu                        \n", count * nthreads / max);

	double allsd = 0;
	double isd = 0;

	for (int i = 0; i < max; i++) {
		double permille = ((double) allhitcount[i]) / (count * nthreads / max) - 1.0;
		allsd += sd[i];
		isd += sq((i + 0.5) - (max * 0.5 - 0.5)) / max;
		printf("[ %2d ] %lu (%+0.7f‰)%c", i, allhitcount[i], 1000.0 * permille, (((i & 3) == 3) || (i == (max - 1)))?'\n':'\t');
	}
	allsd = sqrt(allsd);
	isd = sqrt(isd) * (9.287088 / 9.246621); // no idea why this correction factor is necessary, https://en.wikipedia.org/wiki/Bessel%27s_correction doesn't help because our sample size is _enormous_
	printf("Σ/n (AM): %f / %f (%+0.5f‰)\n", allmean, max * 0.5 - 0.5, 1000.0 * (allmean / (max * 0.5 - 0.5) - 1.0));
	printf("δ   (sd): %f / %f (%+0.5f‰)\n", allsd, isd, 1000.0 * (allsd - isd));

	double runtime = endtime.tv_sec + (endtime.tv_nsec * 0.000000001) - starttime.tv_sec - (starttime.tv_nsec * 0.000000001);
	double rate;
	char prefix;
	siprefix(count * nthreads / runtime, &rate, &prefix);

	printf("Runtime: %8.6fs, %4.2f%c/s\n", runtime, rate, prefix);
}
