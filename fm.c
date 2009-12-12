/* fm.c - simple v4l compatible tuner for radio cards

   Copyright (C) 1998  Russell Kroll <rkroll@exploits.org>

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
#include <getopt.h>
#include <string.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

/* please don't report me to the committee to abolish global variables ...

   they've been removed! */

void help(char *prog)
{
	printf ("usage: %s [-h] [-o] [-q] <freq>|on|off [<volume>]\n", prog); 
	printf ("\n");
	printf ("-h       - display this help\n");
	printf ("-o       - override frequency range limits in card\n");
	printf ("-q       - quiet mode\n");
	printf ("<freq>   - frequency in MHz (i.e. 94.3)\n");
	printf ("on       - turn radio on\n");
	printf ("off      - turn radio off (mute)\n");
	printf ("+        - increase volume\n");
	printf ("-        - decrease volume\n");
	printf ("<volume> - intensity (0-65535)\n");
	
	exit (1);
}

void getconfig(int *defaultvol, int *increment)
{
	FILE	*conf;
	char	buf[256], fn[256];

	sprintf (fn, "%s/.fmrc", getenv("HOME"));
	conf = fopen (fn, "r");

	if (!conf)
		return;

	while (fgets(buf, sizeof(buf), conf)) {
		buf[strlen(buf)-1] = 0;
		if (strncmp (buf, "VOL", 3) == 0)
			sscanf (buf, "%*s %i", defaultvol);
		if (strncmp (buf, "INCR", 3) == 0)
			sscanf (buf, "%*s %i", increment);
	}

	fclose (conf);
	
	return;
}	

int main(int argc, char **argv)
{
	int		fd, ret, tunevol, quiet=0, i, override = 0;
	int		defaultvol = 8192;	/* default volume = 12.5% */
	int		increment = 6554;	/* default change = 10% */
	double		fact;
	unsigned long	freq;
	struct 		video_audio va;
	struct		video_tuner vt;
	char		*progname;
	extern	char	*optarg;
	extern	int	optind, opterr, optopt;

	if ((argc < 2) || (argc > 4)) {
		printf ("usage: %s [-h] [-o] [-q] <freq>|on|off [<volume>]\n", argv[0]); 
		exit (1);
	}

	progname = argv[0];	/* used after getopt munges argv[] */

	fd = open ("/dev/radio0", O_RDONLY); 
	if (fd == -1) {
		perror ("Unable to open /dev/radio0");
		exit (1);
	}

	getconfig(&defaultvol, &increment);

	while ((i = getopt(argc, argv, "+qho")) != EOF) {
		switch (i) {
			case 'q':
				quiet = 1;
				break;
			case 'o':
				override = 1;
			case 'h':
			default:
				help(argv[0]);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 2)
		tunevol = atoi (argv[1]);
	else
		tunevol = defaultvol;

	if (argc == 0)		/* no frequency, on|off, or +|- given */
		help (progname);

	if (!strcmp("off", argv[0])) {		/* mute the radio */
		va.audio = 0;
		va.volume = 0;
		va.flags = VIDEO_AUDIO_MUTE;	
		ret = ioctl (fd, VIDIOCSAUDIO, &va);
		if (!quiet)
			printf ("Radio muted\n");
		exit (0);
	}

	if (!strcmp("on", argv[0])) {		/* turn radio on */
		va.audio = 0;
		va.volume = tunevol;
		va.flags = 0;
		ret = ioctl (fd, VIDIOCSAUDIO, &va);
		if (!quiet)
			printf ("Radio on at %2.2f%% volume\n", 
				(tunevol / 655.36));
		exit (0);
	}

	ret = ioctl (fd, VIDIOCGAUDIO, &va);

	if (argv[0][0] == '+') {		/* volume up */
		if ((va.volume + increment) > 65535)
			va.volume = 65535;	/* catch overflows in __u16 */
		else
			va.volume += increment;

		if (!quiet)
			printf ("Setting volume to %2.2f%%\n", 
				(va.volume / 655.35));
		ret = ioctl (fd, VIDIOCSAUDIO, &va);
		exit (0);
	}

	if (argv[0][0] == '-') {		/* volume down */
		if ((va.volume - increment) < 0) 
			va.volume = 0;		/* catch negative numbers */
		else
			va.volume -= increment;

		if (!quiet)
			printf ("Setting volume to %2.2f%%\n", 
				(va.volume / 655.35));
		ret = ioctl (fd, VIDIOCSAUDIO, &va);
		exit (0);
	}		

	/* at this point, we are trying to tune to a frequency */

	vt.tuner = 0;
	ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get frequency range */

	if (ret == -1 || (vt.flags & VIDEO_TUNER_LOW) == 0)
	  fact = 16.;
	else
	  fact = 16000.;
	freq = ceil(atof(argv[0]) * fact);	/* rounding up matters */

	if ((freq < vt.rangelow) || (freq > vt.rangehigh)) {
		if (override == 0) {
			printf ("Frequency %2.1f out of range (%2.1f - %2.1f MHz)\n",
			        (freq / fact), (vt.rangelow / fact), 
				(vt.rangehigh / fact));
			exit (1);
		}
	}

	/* frequency sanity checked, proceed */
	ret = ioctl (fd, VIDIOCSFREQ, &freq);

	va.audio = 0;
	va.volume = tunevol;
	va.flags = 0;	
	va.mode = VIDEO_SOUND_STEREO;

	ret = ioctl (fd, VIDIOCSAUDIO, &va);	/* set the volume */

	if (!quiet) 
		printf ("Radio tuned to %2.1f MHz at %2.2f%% volume\n", 
			(freq / fact), (tunevol / 655.35));

	return (0);
}
