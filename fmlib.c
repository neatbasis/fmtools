/* fmlib.c - simple V4L2 compatible tuner for radio cards

   Copyright (C) 2009 Ben Pfaff <blp@cs.stanford.edu>

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

#include "fmlib.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct tuner_test {
        int freq;               /* In 1/16 MHz. */
        int volume;             /* Between 1000 and 2000. */
};

char *program_name;

static void query_control(const struct tuner *, uint32_t id,
                          struct v4l2_queryctrl *);
static int32_t get_control(const struct tuner *, uint32_t id);
static void set_control(const struct tuner *, uint32_t id, int32_t value);
static void query_tuner(const struct tuner *, struct v4l2_tuner *);

void
fatal(int error, const char *msg, ...)
{
        va_list args;

        fprintf(stderr, "%s: ", program_name);

        va_start(args, msg);
        vfprintf(stderr, msg, args);
        va_end(args);

        if (error)
                fprintf(stderr, ": %s", strerror(error));
        putc('\n', stderr);

        exit(EXIT_FAILURE);
}

static void *
xmalloc(size_t n)
{
        void *p = malloc(n ? n : 1);
        if (!p)
                fatal(0, "out of memory");
        return p;
}

void
tuner_open(struct tuner *tuner, const char *device, int index)
{
        if (!device)
                device = "/dev/radio0";
        else if (!strcmp(device, "test") || !strncmp(device, "test ", 5)) {
                double volume;

                if (sscanf(device, "test %lf", &volume) != 1)
                        volume = 50;

                tuner->test = xmalloc(sizeof *tuner->test);
                tuner->test->freq = 90 * 16;
                tuner->test->volume = volume * 10 + 1000.5;

                device = "/dev/null";
        }

	tuner->fd = open(device, O_RDONLY);
	if (tuner->fd < 0)
                fatal(errno, "Unable to open %s", device);
        tuner->index = index;

        query_control(tuner, V4L2_CID_AUDIO_VOLUME, &tuner->volume_ctrl);
        query_tuner(tuner, &tuner->tuner);
}

void
tuner_close(struct tuner *tuner)
{
        close(tuner->fd);
}

void
tuner_set_mute(struct tuner *tuner, bool mute)
{
        set_control(tuner, V4L2_CID_AUDIO_MUTE, mute);
}

double
tuner_get_volume(const struct tuner *tuner)
{
        const struct v4l2_queryctrl *vqc = &tuner->volume_ctrl;
        int volume = get_control(tuner, V4L2_CID_AUDIO_VOLUME);
        return 100.0 * (volume - vqc->minimum) / (vqc->maximum - vqc->minimum);
}

void
tuner_set_volume(struct tuner *tuner, double volume)
{
        struct v4l2_queryctrl *vqc = &tuner->volume_ctrl;
        set_control(tuner, V4L2_CID_AUDIO_VOLUME,
                    (volume / 100.0 * (vqc->maximum - vqc->minimum)
                     + vqc->minimum));
}

long long int
tuner_get_min_freq(const struct tuner *tuner)
{
        long long int rangelow = tuner->tuner.rangelow;
        if (!(tuner->tuner.capability & V4L2_TUNER_CAP_LOW))
                rangelow *= 1000;
        return rangelow;
}

long long int
tuner_get_max_freq(const struct tuner *tuner)
{
        long long int rangehigh = tuner->tuner.rangehigh;
        if (!(tuner->tuner.capability & V4L2_TUNER_CAP_LOW))
                rangehigh *= 1000;
        return rangehigh;
}

void
tuner_set_freq(const struct tuner *tuner, long long int freq,
               bool override_range)
{
        long long int adj_freq;
        struct v4l2_frequency vf;

        adj_freq = freq;
        if (!(tuner->tuner.capability & V4L2_TUNER_CAP_LOW))
                adj_freq = (adj_freq + 500) / 1000;

	if ((adj_freq < tuner->tuner.rangelow
             || adj_freq > tuner->tuner.rangehigh)
            && !override_range)
                fatal(0, "Frequency %.1f MHz out of range (%.1f - %.1f MHz)",
                      freq / 16000.0,
                      tuner_get_min_freq(tuner) / 16000.0,
                      tuner_get_max_freq(tuner) / 16000.0);

        memset(&vf, 0, sizeof vf);
        vf.tuner = tuner->index;
        vf.type = tuner->tuner.type;
        vf.frequency = adj_freq;
        if (tuner->test) {
                tuner->test->freq = adj_freq;
        }
        else if (ioctl(tuner->fd, VIDIOC_S_FREQUENCY, &vf) == -1)
                fatal(errno, "VIDIOC_S_FREQUENCY");
}

int
tuner_get_signal(const struct tuner *tuner)
{
        struct v4l2_tuner vt;

        query_tuner(tuner, &vt);
        return vt.signal;
}

void
tuner_usleep(const struct tuner *tuner, int usecs)
{
        if (!tuner->test)
                usleep(usecs);
}

void
tuner_sleep(const struct tuner *tuner, int secs)
{
        if (!tuner->test)
                sleep(secs);
}

static void
query_control(const struct tuner *tuner, uint32_t id,
              struct v4l2_queryctrl *qc)
{
        memset(qc, 0, sizeof *qc);
        qc->id = id;
        if (tuner->test) {
                assert(id == V4L2_CID_AUDIO_VOLUME);
                qc->minimum = 1000;
                qc->maximum = 2000;
        } else if (ioctl(tuner->fd, VIDIOC_QUERYCTRL, qc) == -1)
                fatal(errno, "VIDIOC_QUERYCTRL");
}

static int32_t
get_control(const struct tuner *tuner, uint32_t id)
{
        struct v4l2_control control;

        memset(&control, 0, sizeof control);
        control.id = id;
        if (tuner->test) {
                assert(id == V4L2_CID_AUDIO_VOLUME);
                control.value = tuner->test->volume;
        } else if (ioctl(tuner->fd, VIDIOC_G_CTRL, &control) == -1)
                fatal(errno, "VIDIOC_G_CTRL");
        return control.value;
}

static void
set_control(const struct tuner *tuner, uint32_t id, int32_t value)
{
        struct v4l2_control control;

        memset(&control, 0, sizeof control);
        control.id = id;
        control.value = value;
        if (tuner->test) {
                if (id == V4L2_CID_AUDIO_MUTE)
                        assert(value == 0 || value == 1);
                else if (id == V4L2_CID_AUDIO_VOLUME) {
                        assert(value >= 1000 && value <= 2000);
                        tuner->test->volume = value;
                } else {
                        abort();
                }
        } else if (ioctl(tuner->fd, VIDIOC_S_CTRL, &control) == -1)
                fatal(errno, "VIDIOC_S_CTRL");
}

static void
query_tuner(const struct tuner *tuner, struct v4l2_tuner *vt)
{
        memset(vt, 0, sizeof *vt);
        vt->index = tuner->index;
        if (tuner->test) {
                int freq = tuner->test->freq;
                vt->rangelow = 16 * 89;
                vt->rangehigh = 16 * 91;
                vt->signal = (freq == (int) (16 * 89.6 + .5) ? 64000
                              : freq == (int) (16 * 90.4 + .5) ? 50000
                               : freq == (int) (16 * 90.5 + .5) ? 40000
                              : 1000);
        } else if (ioctl(tuner->fd, VIDIOC_G_TUNER, vt) == -1)
                fatal(errno, "VIDIOC_G_TUNER");
}
