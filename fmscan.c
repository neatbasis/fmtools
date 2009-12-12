/* scan.c - v4l radio band scanner using signal strength

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "videodev.h"

#include "version.h"

#define TRIES		25		/* get 25 samples                 */
#define LOCKTIME	400000		/* wait 400ms for card to lock on */
#define SAMPLEDELAY	15000		/* wait 15ms between samples      */
#define THRESHOLD	.5		/* minimum acceptable signal %    */

void help(char *prog)
{
	printf("fmtools fmscan version %s\n\n", FMT_VERSION);
	printf("usage: %s [-h] [-d <dev>] [-s <freq>] [-e <freq>] [-i <freq>] [-t <%%>]\n\n", prog);

	printf("Auxiliary program to scan a frequency band for radio stations.\n\n");

	printf("  -h        - display this help\n");
	printf("  -d <dev>  - select device (default: /dev/radio0)\n");
	printf("  -s <freq> - set start of scanning range to <freq>\n");
	printf("  -e <freq> - set end of scanning range to <freq>\n");
	printf("  -i <freq> - set increment value between channels to <freq>\n");
	printf("  -t <%%>    - set signal strength percentage to lock onto <%%>\n");
	printf("  <freq>    - a value in the format nnn.nn (MHz)\n");

	exit(0);
}

int main(int argc, char **argv)
{
	int	fd, ret, i, tries = TRIES;
	struct	video_tuner vt;
	float	perc, begval, incval, endval, threshold;
	long	lowf, highf, freq, totsig, incr, fact;
	char	*progname, *dev = NULL;

	progname = argv[0];	/* getopt munges argv[] later */

	/* USA defaults */
	begval = 87.9;		/* start at 87.9 MHz */
	incval = 0.20;		/* increment 0.2 MHz */
	endval = 107.9;		/* stop at 107.9 MHz */

        threshold = THRESHOLD;

	while ((i = getopt(argc, argv, "+e:hi:s:d:t:")) != EOF) {
		switch (i) {
			case 'd':
				dev = strdup(optarg);
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
                        case 't':
                                threshold = atof(optarg)/100.;
                                break;
			case 'h': 
			default:
				help(progname);
				break;
		}
	}

	if (!dev)
		dev = strdup("/dev/radio0");	/* default */

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", dev, strerror(errno));
		exit(1);
	}

	vt.tuner = 0;
	ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get initial info */
	if (ret < 0) {
		perror("ioctl VIDIOCGTUNER");
		exit(1);
	}

	if ((vt.flags & VIDEO_TUNER_LOW) == 0)
		fact = 16;
	else
		fact = 16000;

	/* cope with bizarre things from atof() like 95.099998 */
	lowf = fact * (ceil(rint(begval * 10)) / 10);
	highf = fact * (ceil(rint(endval * 10)) / 10);

	incr = fact * incval;

	printf("Scanning range: %2.1f - %2.1f MHz (%2.1f MHz increments)...\n", 
		begval, endval, incval);

	for (freq = lowf; freq <= highf; freq += incr) {
		ret = ioctl(fd, VIDIOCSFREQ, &freq);		/* tune */

		if (ret < 0) {
			perror("ioctl VIDIOCSFREQ");
			exit(1);
		}

		printf("%2.1f:\r", (freq / (double) fact));
		fflush(stdout);
		usleep(LOCKTIME);		/* let it lock on */
	
		totsig = 0;
		for (i = 1; i < tries+1; i++) {
			vt.tuner = 0;
			ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get info */
			if (ret < 0) {
				perror("ioctl VIDIOCGTUNER");
				exit(1);
			}

			totsig += vt.signal;
			perc = (totsig / (65535.0 * i));

			printf("%2.1f: checking: %3.1f%% (%d/%d)    \r", 
			      (freq / (double) fact), perc * 100.0, i, tries);
			fflush(stdout);
			usleep(SAMPLEDELAY); 
		}

		/* clean up the display */
		printf("                                              \r");	

		perc = (totsig / (65535.0 * tries));

		if (perc > threshold) 
			printf("%2.1f: %3.1f%%          \n", 
			      (freq / (double) fact), perc * 100.0);
	}

	close(fd);
	return 0;
}
