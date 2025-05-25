#include <cstdio>
#include <time.h>

int main()
{
	struct timespec last_ts;
	clock_gettime(CLOCK_MONOTONIC, &last_ts);
	clock_gettime(CLOCK_MONOTONIC, &last_ts);
	struct timespec first_ts = last_ts;
	int differences = 0;
	size_t difference_ns = 0;

	for (int i = 0; i < 2000000; ++i) {
		struct timespec ts;
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
			perror("clock_gettime");
			return 1;
		}
		const __uint128_t nanos_now = static_cast<__uint128_t>(ts.tv_sec) * 1000000000UL + ts.tv_nsec;
		const __uint128_t nanos_last = static_cast<__uint128_t>(last_ts.tv_sec) * 1000000000UL + last_ts.tv_nsec;
		if (nanos_now < nanos_last) {
			fprintf(stderr, "Clock went backwards: now=%ld.%09ld last=%ld.%09ld\n",
				ts.tv_sec, ts.tv_nsec, last_ts.tv_sec, last_ts.tv_nsec);
			return 1;
		} else if (nanos_now < nanos_last) {
			differences++;
			difference_ns += static_cast<size_t>(nanos_last - nanos_now);
		} else if (ts.tv_nsec >= 1000000000UL) {
			fprintf(stderr, "Invalid nanoseconds value: %ld.%09ld\n", ts.tv_sec, ts.tv_nsec);
			return 1;
		} else if (ts.tv_sec < 0 || ts.tv_nsec < 0) {
			fprintf(stderr, "Negative timestamp: %ld.%09ld\n", ts.tv_sec, ts.tv_nsec);
			return 1;
		}
		last_ts = ts;
	}
	printf("Clock is monotonic and did not go backwards. %d differences found (%zuns).\n",
		differences, difference_ns);
	clock_gettime(CLOCK_MONOTONIC, &last_ts);
	const __uint128_t total_nanos = static_cast<__uint128_t>(last_ts.tv_sec - first_ts.tv_sec) * 1000000000UL + (last_ts.tv_nsec - first_ts.tv_nsec);
	printf("Total time: %llu nanoseconds\n",
		static_cast<unsigned long long>(total_nanos));
	// Calculate the average time per iteration
	double average_time = static_cast<double>(total_nanos) / 2000000.0; // in nanoseconds
	printf("Average time per iteration: %.9f nanoseconds\n", average_time);

	// Print the first and last timestamps
	printf("First timestamp: %ld.%09ld\n", first_ts.tv_sec, first_ts.tv_nsec);
	printf("Last timestamp: %ld.%09ld\n", last_ts.tv_sec, last_ts.tv_nsec);

	// Last sanity check, use CLOCK_REALTIME to print the current time
	struct timespec realtime_ts;
	if (clock_gettime(CLOCK_REALTIME, &realtime_ts) == -1) {
		perror("clock_gettime");
		return 1;
	}
	// Print time in a human-readable date format since epoch
	time_t now = realtime_ts.tv_sec;
	char buffer[100];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", gmtime(&now));
	printf("Current time (realtime): %s.%09ld\n", buffer, realtime_ts.tv_nsec);
	if (realtime_ts.tv_sec < first_ts.tv_sec ||
	    (realtime_ts.tv_sec == first_ts.tv_sec && realtime_ts.tv_nsec < first_ts.tv_nsec)) {
		fprintf(stderr, "Realtime clock went backwards: now=%ld.%09ld first=%ld.%09ld\n",
			realtime_ts.tv_sec, realtime_ts.tv_nsec, first_ts.tv_sec, first_ts.tv_nsec);
		return 1;
	}
	printf("Realtime clock is monotonic and did not go backwards.\n");
	return 0;
}
