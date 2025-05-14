#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

const int max = 6;
const long count = 10000000000;

int main(int argc, char** argv) {
	uint64_t hitcount[max] = {};
	uint64_t seed;
	double mean = 0;

	int r = open("/dev/urandom", O_RDONLY);
	if (r >= 0) {
		read(r, &seed, sizeof(seed));
		close(r);
	}
	else {
		seed = 12345;
	}

	for (unsigned long i = 0; i < count; i++) {
		uint32_t rand = ((seed >> 32) * max) >> 32;

		hitcount[rand]++;

		mean += (rand * 1.0) / count;

		seed += (seed * seed) | 5u;

		if ((i & ((1UL << 26) - 1)) == 0) {
			fprintf(stderr, "\r%lu of %lu (%lu%%)", i, count, i * 100 / count);
		}
	}

	fprintf(stderr, "\r%lu of %lu (%lu%%)\n", count, count, 100UL);

	printf("Expected count: %lu                        \n", count / max);

	for (int i = 0; i < max; i++) {
		printf("%lu (%+ld)%c", hitcount[i], hitcount[i] - (count / max), (((i & 3) == 3) || (i == (max - 1)))?'\n':'\t');
	}
	printf("mean: %f, expected %f\n", mean, max * 0.5 - 0.5);
}
