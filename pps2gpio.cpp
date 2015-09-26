#define VERSION "0.1"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timepps.h>
#include <sys/types.h>

#include "error.h"
#include "gpio-int-test.h"

#ifdef __GNUG__
        #define likely(x)       __builtin_expect((x), 1)
        #define unlikely(x)     __builtin_expect((x), 0)
        #define __PRAGMA_PACKED__ __attribute__ ((__packed__))
#else
        #define likely(x) (x)
        #define unlikely(x) (x)
        #define __PRAGMA_PACKED__
#endif

const char *const strings[] = { "0", "1" };

inline void set_value(const int fd, const int fd2, const int value)
{
        write(fd, strings[value], 1);

	write(fd2, strings[value], 1);
}

void usage()
{
	printf("-g x   GPIO pin to use\n");
	printf("-G x   second GPIO pin to use. it'll show the same value but will be set after the -g pin. that way you could measure how long it takes to set a gpio-pin\n");
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
	int gpio_pps_out_pin = -1, out_pin_2 = -1;
	const char *dev = "/dev/pps0";
	int c = -1;
	bool fork = false;

	while((c = getopt(argc, argv, "g:G:p:dhV")) != -1)
	{
		switch(c)
		{
			case 'g':
				gpio_pps_out_pin = atoi(optarg);
				break;

			case 'G':
				out_pin_2 = atoi(optarg);
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

	if (gpio_pps_out_pin == -1)
		error_exit(false, "Need to select a GPIO pin (using -g)");

	if (mlockall(MCL_FUTURE) == -1)
		error_exit(true, "mlockall(MCL_FUTURE) failed");

	bool first_it = true;

	gpio_export(gpio_pps_out_pin);
	gpio_set_dir(gpio_pps_out_pin, 1);
	int gfd = gpio_fd_open(gpio_pps_out_pin), gfd2 = -1;

	if (out_pin_2 != -1)
	{
		gpio_export(out_pin_2);
		gpio_set_dir(out_pin_2, 1);
		gfd2 = gpio_fd_open(out_pin_2);
	}
	else
	{
		gfd2 = open("/dev/null", O_WRONLY);
	}

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

		if (first_it && fork && daemon(0, 0) == -1)
			error_exit(true, "Failed to fork into the background");

		first_it = false;

		bool state = true;

		for(;;)
		{
			static struct timespec timeout = { 3, 0 };
			static pps_info_t infobuf;

			if (unlikely(time_pps_fetch(ph, PPS_TSFMT_TSPEC, &infobuf, &timeout) == -1))
			{
				set_value(gfd, gfd2, false);
				printf("Failed waiting for event\n");
				break;
			}

			set_value(gfd, gfd2, state);

			state = !state;

			static struct timeval tv;
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
