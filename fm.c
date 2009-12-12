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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/ioctl.h>
#include <limits.h>
#include "videodev.h"

#include "version.h"

#define DEFAULT_DEVICE	"/dev/radio0"



static int convert_time (const char *string)
{
	if (strcmp (string, "forever") == 0 ||
            strcmp (string, "-") == 0 ||
            atoi (string) < 0)
		return 0;
	else if (strcmp (string, "none") == 0 ||
                 strcmp (string, "0") == 0)
		return -1;
	else
	{
		char worktime[80+1];
		int inttime;
		const char *suffix;

		suffix = string + strspn (string, "0123456789");

		strncpy (worktime, string, suffix - string);
		worktime[suffix - string] = '\0';
		inttime = atoi (worktime);

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



static char *format_time (char *buffer,
                          const char *string)
{
    if (strcmp (string, "forever") == 0 ||
        strcmp (string, "-") == 0 ||
        atoi (string) < 0)
        strcpy (buffer, "forever");
    else if (strcmp (string, "none") == 0 ||
             strcmp (string, "0") == 0)
        strcpy (buffer, "none");
    else
    {
        char worktime[80+1];
        const char *suffix;
        char *format;
        int int_time;

        suffix = string + strspn (string, "0123456789");
        strncpy (worktime, string, suffix - string);
        worktime[suffix - string] = '\0';
        int_time = atoi (worktime);

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

        sprintf (buffer, format, int_time);
    }

    return buffer;
}



static void maybe_sleep (const char *wait_time)
{
    char message[80+1];
    int int_wait_time;

    int_wait_time = convert_time (wait_time);

    if (int_wait_time > 0)
    {
        printf ("Sleeping for %s\n", format_time (message, wait_time));
        sleep (int_wait_time);
    }
    else if (int_wait_time == 0)
    {
        printf ("Sleeping forever...CTRL-C exits\n");
        do {
            sleep (INT_MAX);
        } while (1);
    }
}



void help(char *prog)
{
	printf("fmtools fm version %s\n\n", FMT_VERSION);
	printf("usage: %s [-h] [-o] [-q] [-d <dev>] [-t <tuner>] "
               "[-T none|forever|time] <freq>|on|off [<volume>]\n\n", 
               prog);
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
	printf("  <volume>   intensity (0-65535)\n");
	exit(0);
}

void getconfig(int *defaultvol, int *increment, char *wait_time)
{
	FILE	*conf;
	char	buf[256], fn[256];

	sprintf(fn, "%s/.fmrc", getenv("HOME"));
	conf = fopen(fn, "r");

	if (!conf)
		return;

	while(fgets(buf, sizeof(buf), conf)) {
		buf[strlen(buf)-1] = 0;
		if (!strncmp(buf, "VOL", 3))
			sscanf(buf, "%*s %i", defaultvol);
		if (!strncmp(buf, "INCR", 3))
			sscanf(buf, "%*s %i", increment);
		if (!strncmp(buf, "TIME", 4))
			sscanf(buf, "%*s %s", wait_time);
	}

	fclose(conf);
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
	int		tuner = 0;
        char		wait_time_buf[256+1] = "";
	char		*wait_time = "none";
	char	*dev = NULL;

	/* need at least a frequency */
	if (argc < 2) {
		printf("usage: %s [-h] [-o] [-q] [-d <dev>] [-t <tuner>] "
			"[-T time|forever|none] "
			"<freq>|on|off [<volume>]\n", argv[0]);
		exit(1);
	}

	progname = argv[0];	/* used after getopt munges argv[] */

	getconfig(&defaultvol, &increment, wait_time_buf);
        if (*wait_time_buf)
            wait_time = wait_time_buf;

	while ((i = getopt(argc, argv, "+qhot:T:d:")) != EOF) {
		switch (i) {
			case 'q':
				quiet = 1;
				break;
			case 'o':
				override = 1;
				break;
			case 't':
				tuner = atoi(optarg);
				break;
			case 'T':
				wait_time = optarg;
				break;
			case 'd':
				dev = strdup(optarg);
				break;
			case 'h':
			default:
				help(argv[0]);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)		/* no frequency, on|off, or +|- given */
		help(progname);

	if (argc == 2)
		tunevol = atoi(argv[1]);
	else
		tunevol = defaultvol;

	if (!dev)
		dev = DEFAULT_DEVICE;

	fd = open(dev, O_RDONLY); 
	if (fd < 0) {
		fprintf(stderr, "Unable to open %s: %s\n", 
			dev, strerror(errno));
		exit(1);
	}

	ret = ioctl(fd, VIDIOCGAUDIO, &va);
	if (ret < 0) {
		perror("ioctl VIDIOCGAUDIO");
		exit(1);
	}

	/* initialize this so it doesn't pick up random junk data */
	va.balance = 32768;

	if (!strcmp("off", argv[0])) {		/* mute the radio */
		va.audio = 0;
		va.volume = 0;
		va.flags = VIDEO_AUDIO_MUTE;	
		ret = ioctl(fd, VIDIOCSAUDIO, &va);
		if (ret < 0) {
			perror("ioctl VIDIOCSAUDIO");
			exit(1);
		}
		
		if (!quiet)
			printf("Radio muted\n");
		exit(0);
	}

	if (!strcmp("on", argv[0])) {		/* turn radio on */
		va.audio = 0;
		va.volume = tunevol;
		va.flags = 0;
		ret = ioctl(fd, VIDIOCSAUDIO, &va);
		if (ret < 0) {
			perror("ioctl VIDIOCSAUDIO");
			exit(1);
		}

		if (!quiet)
			printf("Radio on at %2.2f%% volume\n", 
			      (tunevol / 655.36));
		maybe_sleep (wait_time);
		exit(0);
	}

	if (argv[0][0] == '+') {		/* volume up */
		if ((va.volume + increment) > 65535)
			va.volume = 65535;	/* catch overflows in __u16 */
		else
			va.volume += increment;

		if (!quiet)
			printf("Setting volume to %2.2f%%\n", 
			      (va.volume / 655.35));

		ret = ioctl(fd, VIDIOCSAUDIO, &va);
		if (ret < 0) {
			perror("ioctl VIDIOCSAUDIO");
			exit(1);
		}

		maybe_sleep (wait_time);
		exit(0);
	}

	if (argv[0][0] == '-') {		/* volume down */
		if ((va.volume - increment) < 0) 
			va.volume = 0;		/* catch negative numbers */
		else
			va.volume -= increment;

		if (!quiet)
			printf("Setting volume to %2.2f%%\n", 
				(va.volume / 655.35));

		ret = ioctl(fd, VIDIOCSAUDIO, &va);
		if (ret < 0) {
			perror("ioctl VIDIOCSAUDIO");
			exit(1);
		}

		maybe_sleep (wait_time);
		exit(0);
	}		

	/* at this point, we are trying to tune to a frequency */

	vt.tuner = tuner;
	ret = ioctl(fd, VIDIOCSTUNER, &vt);	/* set tuner # */
	if (ret < 0) {
		perror("ioctl VIDIOCSTUNER");
		exit(1);
	}

	ret = ioctl(fd, VIDIOCGTUNER, &vt);	/* get frequency range */
	if (ret < 0) {
		perror("ioctl VIDIOCGTUNER");
		exit(1);
	}

	if ((ret == -1) || ((vt.flags & VIDEO_TUNER_LOW) == 0))
		fact = 16.;
	else
		fact = 16000.;

	freq = rint(atof(argv[0]) * fact);	/* rounding to nearest int */

	if ((freq < vt.rangelow) || (freq > vt.rangehigh)) {
		if (override == 0) {
			printf("Frequency %2.1f out of range (%2.1f - %2.1f MHz)\n",
			      (freq / fact), (vt.rangelow / fact), 
			      (vt.rangehigh / fact));
			exit(1);
		}
	}

	/* frequency sanity checked, proceed */
	ret = ioctl(fd, VIDIOCSFREQ, &freq);
	if (ret < 0) {
		perror("ioctl VIDIOCSFREQ");
		exit(1);
	}

	va.audio = 0;
	va.volume = tunevol;
	va.flags = 0;	
	va.mode = VIDEO_SOUND_STEREO;

	ret = ioctl(fd, VIDIOCSAUDIO, &va);	/* set the volume */
	if (ret < 0) {
		perror("ioctl VIDIOCSAUDIO");
		exit(1);
	}

	if (!quiet) 
		printf("Radio tuned to %2.2f MHz at %2.2f%% volume\n", 
			(freq / fact), (tunevol / 655.35));

	maybe_sleep (wait_time);
	return 0;
}
