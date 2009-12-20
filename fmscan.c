/* fmscan.c - v4l radio band scanner using signal strength

   Copyright (C) 2004, 2006, 2009 Ben Pfaff <blp@cs.stanford.edu>
   Copyright (C) 1999 Russell Kroll <rkroll@exploits.org>

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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "fmlib.h"

#define TRIES		25		/* get 25 samples                 */
#define LOCKTIME	400000		/* wait 400ms for card to lock on */
#define SAMPLEDELAY	15000		/* wait 15ms between samples      */

static void
usage(void)
{
	printf("fmtools fmscan version %s\n\n", VERSION);
	printf("usage: %s [-h] [-d <dev>] [-T <tuner>] [-s <freq>] [-e <freq>] [-i <freq>] [-t <%%>]\n\n", program_name);

	printf("Auxiliary program to scan a frequency band for radio stations.\n\n");

	printf("  -h        - display this help\n");
	printf("  -d <dev>  - select device (default: /dev/radio0)\n");
	printf("  -T <tuner> - select tuner (default: 0)\n");
	printf("  -s <freq> - set start of scanning range to <freq>\n");
	printf("  -e <freq> - set end of scanning range to <freq>\n");
	printf("  -i <freq> - set increment value between channels to <freq>\n");
	printf("  -t <%%>    - set signal strength percentage to lock onto <%%>\n");
	printf("  <freq>    - a value in the format nnn.nn (MHz)\n");

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
        struct tuner tuner;
        int tries = TRIES;
	double perc, begval, incval, endval, threshold, mhz;
	const char *device = NULL;
        bool override = false;
        bool quiet = false;
        int index = 0;
        int i;

        program_name = argv[0];

	/* USA defaults */
	begval = 87.9;		/* start at 87.9 MHz */
	incval = 0.20;		/* increment 0.2 MHz */
	endval = 107.9;		/* stop at 107.9 MHz */
        threshold = 0.5;        /* 50% signal strength */

        for (;;) {
                int option = getopt(argc, argv, "+e:hi:s:od:T:t:q");
                if (option == -1)
                        break;

		switch (option) {
                case 'd':
                        device = optarg;
                        break;
                case 'T':
                        index = atoi(optarg);
                        break;
                case 'e':
                        endval = atof(optarg);
                        break;
                case 'i':
                        incval = atof(optarg);
                        break;
                case 's':
                        begval = atof(optarg);
                        break;
                case 'o':
                        override = true;
                        break;
                case 't':
                        threshold = atof(optarg)/100.;
                        break;
                case 'q':
                        quiet = true;
                        break;
                case 'h':
                default:
                        usage();
                        break;
		}
	}

        tuner_open(&tuner, device, index);

        if (!override) {
                double min = tuner_get_min_freq(&tuner) / 16000.0;
                double max = tuner_get_max_freq(&tuner) / 16000.0;
                if (begval < min) {
                        begval = min;
                        printf("Setting start to tuner minimum %.1f MHz\n",
                               begval);
                }
                if (endval > max) {
                        endval = max;
                        printf("Setting end to tuner maximum %.1f MHz\n",
                               endval);
                }
        }

	printf("Scanning range: %2.1f - %2.1f MHz (%2.1f MHz increments)...\n",
               begval, endval, incval);

	for (i = 0; (mhz = begval + i * incval) <= endval; i++) {
                long long int freq;
                long int totsig;
                int i;

                freq = mhz * 16000 + 0.5;
                if (!override) {
                        if (freq < tuner_get_min_freq(&tuner))
                                freq = tuner_get_min_freq(&tuner);
                        else if (freq > tuner_get_max_freq(&tuner))
                                freq = tuner_get_max_freq(&tuner);
                }
                tuner_set_freq(&tuner, freq, override);

                if (!quiet) {
                        printf("%2.1f:\r", mhz);
                        fflush(stdout);
                }
                tuner_usleep(&tuner, LOCKTIME);

		totsig = 0;
		for (i = 0; i < tries; i++) {
                        totsig += tuner_get_signal(&tuner);
			perc = totsig / (65535.0 * (i + 1));
                        if (!quiet) {
                                printf("%2.1f: checking: %3.1f%% (%d/%d)"
                                       "    \r",
                                       mhz, perc * 100.0, i + 1, tries);
                                fflush(stdout);
                        }
			tuner_usleep(&tuner, SAMPLEDELAY);
		}

		/* clean up the display */
                if (!quiet)
                        printf("                                          \r");

		perc = totsig / (65535.0 * tries);

		if (perc > threshold)
			printf("%2.1f: %3.1f%%\n",
                               mhz, perc * 100.0);
	}

        tuner_close(&tuner);
	return 0;
}
