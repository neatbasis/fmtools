/* fm.c - simple V4L2 compatible tuner for radio cards

   Copyright (C) 2004, 2006, 2009 Ben Pfaff <blp@cs.stanford.edu>
   Copyright (C) 1998 Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

#include "fmlib.h"

#define DEFAULT_DEVICE	"/dev/radio0"

static double
clamp(double percent)
{
        return percent < 0.0 ? 0.0 : percent > 100.0 ? 100.0 : percent;
}

static int
convert_time (const char *string)
{
	if (strcmp(string, "forever") == 0 ||
            strcmp(string, "-") == 0 ||
            atoi(string) < 0)
		return 0;
	else if (strcmp(string, "none") == 0 ||
                 strcmp(string, "0") == 0)
		return -1;
	else
	{
		char worktime[80+1];
		int inttime;
		const char *suffix;

		suffix = string + strspn(string, "0123456789");

		strncpy(worktime, string, suffix - string);
		worktime[suffix - string] = '\0';
		inttime = atoi(worktime);

		switch (*suffix)
		{
		case 's':
		case '\0':
			break;
		case 'm':
			inttime *= 60;
			break;
		case 'h':
			inttime *= 60 * 60;
			break;
		case 'd':
			inttime *= 24 * 60 * 60;
			break;
		default:
			break;
		}

		return inttime;
	}
}



static char *
format_time (char *buffer, const char *string)
{
        if (strcmp(string, "forever") == 0 ||
            strcmp(string, "-") == 0 ||
            atoi(string) < 0)
                strcpy(buffer, "forever");
        else if (strcmp(string, "none") == 0 ||
                 strcmp(string, "0") == 0)
                strcpy(buffer, "none");
        else
        {
                char worktime[80+1];
                const char *suffix;
                char *format;
                int int_time;

                suffix = string + strspn(string, "0123456789");
                strncpy(worktime, string, suffix - string);
                worktime[suffix - string] = '\0';
                int_time = atoi(worktime);

                switch (*suffix)
                {
                case 'm':
                        format = "%d minute(s)";
                        break;
                case 'h':
                        format = "%d hour(s)";
                        break;
                case 'd':
                        format = "%d day(s)";
                        break;
                case 's':
                case '\0':
                default:
                        format = "%d second(s)";
                break;
                }

                sprintf(buffer, format, int_time);
        }

        return buffer;
}

static void
maybe_sleep(const struct tuner *tuner, const char *wait_time)
{
        char message[80+1];
        int int_wait_time;

        int_wait_time = convert_time(wait_time);

        if (int_wait_time > 0)
        {
                printf("Sleeping for %s\n", format_time(message, wait_time));
                tuner_sleep(tuner, int_wait_time);
        } else if (int_wait_time == 0)
        {
                printf("Sleeping forever...CTRL-C exits\n");
                for (;;)
                        pause();
        }
}

static void
usage(void)
{
	printf("fmtools fm version %s\n\n", VERSION);
	printf("usage: %s [-h] [-o] [-q] [-d <dev>] [-t <tuner>] "
               "[-T none|forever|time] <freq>|on|off [<volume>]\n\n", 
               program_name);
	printf("A small controller for Video for Linux radio devices.\n\n");
	printf("  -h         display this help\n");
	printf("  -o         override frequency range limits of card\n");
	printf("  -q         quiet mode\n");
	printf("  -d <dev>   select device (default: /dev/radio0)\n");
	printf("  -t <tuner> select tuner (default: 0)\n");
        printf("  -T <time>  after setting frequency, sleep for some time\n"\
               "             (default: none; -=forever)\n");
	printf("  <freq>     frequency in MHz (i.e. 94.3)\n");
	printf("  on         turn radio on\n");
	printf("  off        turn radio off (mute)\n");
	printf("  +          increase volume\n");
	printf("  -          decrease volume\n");
	printf("  <volume>   percentage (0-100)\n");
	exit(EXIT_SUCCESS);
}

static void
getconfig(const char *fn,
          double *defaultvol, double *increment, char *wait_time)
{
	FILE	*conf;
	char	buf[256];

        if (!fn) {
                snprintf(buf, sizeof buf, "%s/.fmrc", getenv("HOME"));
                fn = buf;
        }
	conf = fopen(fn, "r");

	if (!conf)
		return;

	while(fgets(buf, sizeof(buf), conf)) {
		buf[strlen(buf)-1] = 0;
		if (!strncmp(buf, "VOL", 3))
			sscanf(buf, "%*s %lf", defaultvol);
		if (!strncmp(buf, "INCR", 3))
			sscanf(buf, "%*s %lf", increment);
		if (!strncmp(buf, "TIME", 4))
			sscanf(buf, "%*s %s", wait_time);
	}

	fclose(conf);
}

static void
print_volume(const struct tuner *tuner)
{
        if (tuner_has_volume_control(tuner))
                printf(" at %.2f%% volume", tuner_get_volume(tuner));
        else
                printf(" (radio does not support volume control)");
}

static void
print_mute(const struct tuner *tuner)
{
        if (tuner_is_muted(tuner))
                printf(" (radio is muted, use \"fm on\" to unmute)");
}

int main(int argc, char **argv)
{
        struct tuner tuner;
	const char *device = NULL;
        int index = 0;
        bool quiet = false;
        bool override = false;
        const char *config_file = NULL;
	double defaultvol = 12.5;
	double increment = 10;
        char wait_time_buf[256+1] = "";
	const char *wait_time = NULL;

        program_name = argv[0];

	/* need at least a frequency */
	if (argc < 2)
		fatal(0, "usage: %s [-h] [-o] [-q] [-d <dev>] [-t <tuner>] "
                      "[-T time|forever|none] "
                      "<freq>|on|off [<volume>]\n", program_name);

        for (;;) {
                int option = getopt(argc, argv, "+qhot:T:c:d:");
                if (option == -1)
                        break;

		switch (option) {
                case 'q':
                        quiet = 1;
                        break;
                case 'o':
                        override = 1;
                        break;
                case 't':
                        index = atoi(optarg);
                        break;
                case 'T':
                        wait_time = optarg;
                        break;
                case 'd':
                        device = optarg;
                        break;
                case 'c':
                        config_file = optarg;
                        break;
                case 'h':
                default:
                        usage();
                        break;
		}
	}

	getconfig(config_file, &defaultvol, &increment, wait_time_buf);
        if (!wait_time)
                wait_time = *wait_time_buf ? wait_time_buf : "none";

	argc -= optind;
	argv += optind;

	if (argc == 0)		/* no frequency, on|off, or +|- given */
		usage();

        tuner_open(&tuner, device, index);

        if (!strcmp(argv[0], "off")) {
                tuner_set_mute(&tuner, true);
                if (!quiet)
                        printf("Radio muted\n");
        } else if (!strcmp(argv[0], "on")) {
                tuner_set_mute(&tuner, false);
                if (!quiet) {
                        printf("Radio on");
                        print_volume(&tuner);
                        putchar('\n');
                }
        } else if (!strcmp(argv[0], "+") || !strcmp(argv[0], "-")) {
                double new_volume;
                if (!tuner_has_volume_control(&tuner))
                        fatal(0, "Radio does not support volume control");

                new_volume = tuner_get_volume(&tuner);
                if (argv[0][0] == '+')
                        new_volume += increment;
                else
                        new_volume -= increment;
                new_volume = clamp(new_volume);

                tuner_set_volume(&tuner, new_volume);

                if (!quiet) {
                        printf("Setting volume to %.2f%%", new_volume);
                        print_mute(&tuner);
                        putchar('\n');
                }
        } else if (atof(argv[0])) {
                double frequency = atof(argv[0]);
                double volume = argc > 1 ? clamp(atof(argv[1])) : defaultvol;
                tuner_set_freq(&tuner, frequency * 16000.0, override);
                tuner_set_volume(&tuner, volume);
                if (!quiet) {
                        printf("Radio tuned to %2.2f MHz", frequency);
                        print_volume(&tuner);
                        print_mute(&tuner);
                        putchar('\n');
                }
        } else {
                fatal(0, "unrecognized command syntax; use --help for help");
        }
        maybe_sleep(&tuner, wait_time);
        tuner_close(&tuner);
	return 0;
}
