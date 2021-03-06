/*
  Copyright (C) 2003-2013 Paul Brossier <piem@aubio.org>

  This file is part of aubio.

  aubio is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  aubio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with aubio.  If not, see <http://www.gnu.org/licenses/>.

*/

/**

  This file includes some tools common to all examples. Code specific to the
  algorithm performed by each program should go in the source file of that
  program instead.

*/

#include "utils.h"
#ifdef HAVE_JACK
#include "jackio.h"
#endif /* HAVE_JACK */

int verbose = 0;
int usejack = 0;
// input / output
char_t *sink_uri = NULL;
char_t *source_uri = NULL;
// general stuff
uint_t samplerate = 0;
uint_t buffer_size = 512;
uint_t hop_size = 256;
// onset stuff
char_t * onset_method = "default";
smpl_t onset_threshold = 0.0; // will be set if != 0.
// pitch stuff
char_t * pitch_unit = "default";
char_t * pitch_method = "default";
smpl_t pitch_tolerance = 0.0; // will be set if != 0.
// tempo stuff
char_t * tempo_method = "default";
// more general stuff
smpl_t silence_threshold = -90.;
uint_t mix_input = 0;

uint_t force_overwrite = 0;

//
// internal memory stuff
aubio_source_t *this_source = NULL;
aubio_sink_t *this_sink = NULL;
fvec_t *ibuf;
fvec_t *obuf;

/* settings */
int blocks = 0;

extern void usage (FILE * stream, int exit_code);
extern int parse_args (int argc, char **argv);

#if HAVE_JACK
aubio_jack_t *jack_setup;
#endif

void
examples_common_init (int argc, char **argv)
{

  /* parse command line arguments */
  parse_args (argc, argv);

  if (!usejack) {
    debug ("Opening files ...\n");
    this_source = new_aubio_source ((char_t*)source_uri, samplerate, hop_size);
    if (this_source == NULL) {
      errmsg ("Error: could not open input file %s\n", source_uri);
      exit (1);
    }
    if (samplerate == 0) {
      samplerate = aubio_source_get_samplerate(this_source);
    }
    if (sink_uri != NULL) {
      uint_t sink_exists = (access(sink_uri, F_OK) == 0 );
      if (!force_overwrite && sink_exists) {
        errmsg ("Error: output file %s already exists, use -f to overwrite.\n",
            sink_uri);
        exit (1);
      }
      this_sink = new_aubio_sink ((char_t*)sink_uri, samplerate);
      if (this_sink == NULL) {
        errmsg ("Error: could not create output file %s\n", sink_uri);
        exit (1);
      }
    }
#ifdef HAVE_JACK
  } else {
    debug ("Jack init ...\n");
    jack_setup = new_aubio_jack (hop_size, 1, 1, 0, 1);
    samplerate = aubio_jack_get_samplerate (jack_setup);
    source_uri = "jack";
#endif
  }
  ibuf = new_fvec (hop_size);
  obuf = new_fvec (hop_size);

}

void
examples_common_del (void)
{
  del_fvec (ibuf);
  del_fvec (obuf);
  aubio_cleanup ();
  fflush(stderr);
  fflush(stdout);
}

void
examples_common_process (aubio_process_func_t process_func,
    aubio_print_func_t print)
{

  uint_t read = 0;
  if (usejack) {

#if HAVE_JACK
    debug ("Jack activation ...\n");
    aubio_jack_activate (jack_setup, process_func);
    debug ("Processing (Ctrl+C to quit) ...\n");
    pause ();
    aubio_jack_close (jack_setup);
#else
    usage (stderr, 1);
    outmsg ("Compiled without jack output, exiting.\n");
#endif

  } else {
    /* phasevoc */
    blocks = 0;
    uint_t total_read = 0;

    do {
      aubio_source_do (this_source, ibuf, &read);
      process_func (ibuf, obuf);
      // print to console if verbose or no output given
      if (verbose || sink_uri == NULL) {
        print();
      }
      if (this_sink) {
        aubio_sink_do (this_sink, obuf, hop_size);
      }
      blocks++;
      total_read += read;
    } while (read == hop_size);

    verbmsg ("read %.2fs (%d samples in %d blocks of %d) from %s at %dHz\n",
        total_read * 1. / samplerate,
        total_read, blocks, hop_size, source_uri, samplerate);

    del_aubio_source (this_source);
    del_aubio_sink   (this_sink);

  }
}

void
send_noteon (int pitch, int velo)
{
  smpl_t mpitch = floor (aubio_freqtomidi (pitch) + .5);
#if HAVE_JACK
  jack_midi_event_t ev;
  ev.size = 3;
  ev.buffer = malloc (3 * sizeof (jack_midi_data_t)); // FIXME
  ev.time = 0;
  if (usejack) {
    ev.buffer[2] = velo;
    ev.buffer[1] = mpitch;
    if (velo == 0) {
      ev.buffer[0] = 0x80;      /* note off */
    } else {
      ev.buffer[0] = 0x90;      /* note on */
    }
    aubio_jack_midi_event_write (jack_setup, (jack_midi_event_t *) & ev);
  } else
#endif
  if (velo == 0) {
    verbmsg ("%f\n", blocks * hop_size / (float) samplerate);
  } else {
    verbmsg ("%f\t%f\t", mpitch, blocks * hop_size / (float) samplerate);
  }
}

