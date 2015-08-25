#define VERSION "0.1"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "gpio-int-test.h"

#include <sys/timepps.h>

inline void set_value(const int fd, const int value)
{
        static char c[32];
        int l = snprintf(c, sizeof c, "%d", value);

        write(fd, c, l);
}

void usage()
{
	printf("-g x   GPIO pin to use\n");
	printf("-p x   pps-device to use. defaults to pps0\n");
	printf("-d     fork into the background\n");
	printf("-h     this help\n");
	printf("-V     show version\n");
}

void version()
{
	printf("pps2gpio v" VERSION ", (C) 2015 by folkert@vanheusden.com\n\n");
}

int main(int argc, char *argv[0])
{
	int gpio_pps_out_pin = -1;
	const char *dev = "/dev/pps0";
	int c = -1;
	bool fork = false;

	while((c = getopt(argc, argv, "g:p:dhV")) != -1)
	{
		switch(c)
		{
			case 'g':
				gpio_pps_out_pin = atoi(optarg);
				break;

			case 'p':
				dev = optarg;
				break;

			case 'd':
				fork = true;
				break;

			case 'h':
				usage();
				return 0;

			case 'V':
				version();
				return 0;

			default:
				usage();
				return 1;
		}
	}

	bool first_it = true;

	for(;;)
	{
		int fd = open(dev, O_RDWR);
		if (fd == -1)
			error_exit(true, "Cannot access %s", dev);

		pps_handle_t ph;
		if (time_pps_create(fd, &ph) == -1)
			error_exit(true, "Cannot access pps device for %s", dev);

		/* Find out what features are supported */
		int mode = 0;
		if (time_pps_getcap(ph, &mode) == -1)
			error_exit(true, "Failed retrieving pps device capabilities");

		if ((mode & PPS_CAPTUREASSERT) == 0)
			error_exit(false, "PPS_CAPTUREASSERT not available");

		if ((mode & PPS_CANWAIT) == 0)
			error_exit(false, "PPS_CANWAIT not available");

		pps_params_t params;
		if (time_pps_getparams(ph, &params) == -1)
			error_exit(true, "time_pps_getparams failed");

		params.mode |= PPS_CAPTUREASSERT;

		if (time_pps_setparams(ph, &params) == -1)
			error_exit(true, "time_pps_setparams failed");

		gpio_export(gpio_pps_out_pin);
		gpio_set_dir(gpio_pps_out_pin, 1);
		int gfd = gpio_fd_open(gpio_pps_out_pin);

		if (first_it && fork && daemon(0, 0) == -1)
			error_exit(true, "Failed to fork into the background");

		first_it = false;

		bool state = false;

		for(;;)
		{
			struct timespec timeout = { 3, 0 };
			pps_info_t infobuf;

			if (time_pps_fetch(ph, PPS_TSFMT_TSPEC, &infobuf, &timeout) == -1)
			{
				set_value(gfd, false);
				printf("Failed waiting for event\n");
				break;
			}

			state = !state;
			set_value(gfd, state);

			struct timeval tv;
			if (gettimeofday(&tv, NULL) == -1)
				error_exit(true, "gettimeofday failed");

			printf("%ld.%06ld\n", tv.tv_sec, tv.tv_usec);
		}

		close(gfd);

		time_pps_destroy(ph);

		close(fd);
	}

	return 0;
}
