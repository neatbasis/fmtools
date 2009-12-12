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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

void help (char *prog)
{
	printf ("usage: %s [-h] [-s <freq>] [-e <freq>] [-i <freq>]\n\n", prog);

	printf ("-h        - display this help\n");
	printf ("-s <freq> - set start of scanning range to <freq>\n");
	printf ("-e <freq> - set end of scanning range to <freq>\n");
	printf ("-i <freq> - set increment value between channels to <freq>\n");
	printf ("<freq>    - a value in the format nnn.nn (MHz)\n");

	exit (1);
}

int main(int argc, char **argv)
{
	int	fd, ret, i, tries = 25;
	struct	video_tuner vt;
	float	perc, begval, incval, endval;
	long	lowf, highf, freq, totsig, incr, fact;
	char	*progname;

	progname = argv[0];	/* getopt munges argv[] later */

	fd = open ("/dev/radio0", O_RDONLY); 
	if (fd == -1) {
		perror ("Unable to open /dev/radio0");
		exit (1);
	}

	/* USA defaults */
	begval = 87.9;		/* start at 87.9 MHz */
	incval = 0.20;		/* increment 0.2 MHz */
	endval = 107.9;		/* stop at 107.9 MHz */

	while ((i = getopt(argc, argv, "+e:hi:s:")) != EOF) {
		switch (i) {
			case 'e':
				endval = atof (optarg);
				break;
			case 'i':
				incval = atof (optarg);
				break;
			case 's':
				begval = atof (optarg);
				break;
			case 'h': 
			default:
				help(progname);
				break;
		}
	}

	vt.tuner = 0;
	ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get initial info */

	if ((vt.flags & VIDEO_TUNER_LOW) == 0)
		fact = 16;
	else
		fact = 16000;

	/* cope with bizarre things from atof() like 95.099998 */
	lowf = fact * (ceil(rint(begval * 10)) / 10);
	highf = fact * (ceil(rint(endval * 10)) / 10);

	incr = fact * incval;

	printf ("Scanning range: %2.1f - %2.1f MHz (%2.1f MHz increments)...\n", 
		begval, endval, incval);

	for (freq = lowf; freq <= highf; freq += incr) {
		ioctl (fd, VIDIOCSFREQ, &freq);		/* tune */

		printf ("%2.1f: checking\r", (freq / (double) fact));

		fflush (stdout);
		usleep (400000);		/* let it lock on */
	
		totsig = 0;
		for (i = 0; i < tries; i++) {
			vt.tuner = 0;
			ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get info */
			totsig += vt.signal;
			fflush(stdout);
			usleep (15000); 
		}

		/* clean up the display */
		printf ("                  \r");	

		perc = (totsig / (65535.0 * tries));

		if (perc > .5) 
			printf ("%2.1f: %3.1f%%          \n", 
				(freq / (double) fact), perc * 100.0);
	}

	close (fd);

	return (0);
}
