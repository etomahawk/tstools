/*
 * Report on the contents of an H.264 (MPEG-4/AVC) or H.262 (MPEG-2)
 * elementary stream, as a sequence of single characters, representing
 * appropriate entities.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MPEG TS, PS and ES tools.
 *
 * The Initial Developer of the Original Code is Amino Communications Ltd.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Amino Communications Ltd, Swavesey, Cambridge UK
 *
 * ***** END LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#include <stddef.h>
#else  // _WIN32
#include <unistd.h>
#endif // _WIN32


#include "compat.h"
#include "es_fns.h"
#include "pes_fns.h"
#include "accessunit_fns.h"
#include "h262_fns.h"
#include "avs_fns.h"
#include "misc_fns.h"
#include "version.h"


/*
 * Print out a single character representative of our item.
 */
static void h262_item_dot(h262_item_p  item)
{
  char *str = NULL;

  static int frames = 0;
  if (item->unit.start_code == 0x00)
  {
    if (frames % (25*60) == 0)
      printf("\n%d minute%s\n",frames/(25*60),(frames/(25*60)==1?"":"s"));
    frames ++;
  }

  switch (item->unit.start_code)
  {
  case 0x00:
    str = (item->picture_coding_type==1?"i":
           item->picture_coding_type==2?"p":
           item->picture_coding_type==3?"b":
           item->picture_coding_type==4?"d":"x");
    break;
  case 0xB0: str = "R"; break; // Reserved
  case 0xB1: str = "R"; break; // Reserved
  case 0xB2: str = "U"; break; // User data
  case 0xB3: str = "["; break; // SEQUENCE HEADER
  case 0xB4: str = "X"; break; // Sequence error
  case 0xB5: str = "E"; break; // Extension start
  case 0xB6: str = "R"; break; // Reserved
  case 0xB7: str = "]"; break; // SEQUENCE END
  case 0xB8: str = ">"; break; // Group start

  default:
    if (str == NULL)
    {
      if (item->unit.start_code >= 0x01 && item->unit.start_code <= 0xAF)
        return; //str = "."; // Don't report slice data explicitly
      else
        str = "?";
    }
    break;
  }
  printf(str);
  fflush(stdout);
}

/*
 * Simply report on the content of an MPEG2 file as single characters
 *
 * - `es` is the input elementary stream
 * - if `max` is non-zero, then reporting will stop after `max` MPEG items
 * - if `verbose` is true, then extra information will be output
 *
 * Returns 0 if it succeeds, 1 if some error occurs.
 */
static int report_h262_file_as_dots(ES_p    es,
                                    int     max,
                                    int     verbose)
{
  int  err;
  int  count = 0;

  if (verbose)
    printf("\n"
           "Each character represents a single H.262 item\n"
           "Pictures are represented according to their picture coding\n"
           "type, and the slices within a picture are not shown.\n"
           "    i means an I picture\n"
           "    p means a  P picture\n"
           "    b means a  B picture\n"
           "    d means a  D picture (these should not occur in MPEG-2)\n"
           "    x means some other picture (such should not occur)\n"
           "Other items are represented as follows:\n"
           "    [ means a  Sequence header\n"
           "    > means a  Group Start header\n"
           "    E means an Extension start header\n"
           "    U means a  User data header\n"
           "    X means a  Sequence Error\n"
           "    ] means a  Sequence End\n"
           "    R means a  Reserved item\n"
           "    ? means something else. This may indicate that the stream\n"
           "      is not an ES representing H.262 (it might, for instance\n"
           "      be PES)\n"
           "\n");
  
  for (;;)
  {
    h262_item_p  item;

    err = find_next_h262_item(es,&item);
    if (err == EOF)
      break;
    else if (err)
    {
      fprintf(stderr,"### Error copying NAL units\n");
      return err;
    }
    count++;
    h262_item_dot(item);
    free_h262_item(&item);
    
    if (max > 0 && count >= max)
      break;
  }
  printf("\nFound %d MPEG2 item%s\n",count,(count==1?"":"s"));
  return 0;
}

/*
 * Simply report on the content of an AVS file as single characters
 *
 * - `es` is the input elementary stream
 * - if `max` is non-zero, then reporting will stop after `max` MPEG items
 * - if `verbose` is true, then extra information will be output
 *
 * Returns 0 if it succeeds, 1 if some error occurs.
 */
static int report_avs_file_as_dots(ES_p    es,
                                   int     max,
                                   int     verbose)
{
  int            err = 0;
  int            count = 0;
  int            frames = 0;
  double         frame_rate = 25.0;      // as a guess
  avs_context_p  context;

  if (verbose)
    printf("\n"
           "Each character represents a single AVS item\n"
           "Frames are represented according to their picture coding\n"
           "type, and the slices within a frame are not shown.\n"
           "    i means an I frame\n"
           "    p means a  P frame\n"
           "    b means a  B frame\n"
           "    _ means a (stray) slice, normally only at the start of a stream\n"
           "    ! means something else (this should not be possible)\n"
           "Other items are represented as follows:\n"
           "    [ means a  Sequence header\n"
           "    E means an Extension start header\n"
           "    U means a  User data header\n"
           "    ] means a  Sequence End\n"
           "    V means a  Video edit item\n"
           "    ? means something else. This may indicate that the stream\n"
           "      is not an ES representing AVS (it might, for instance\n"
           "      be PES)\n"
           "\n");
  
  err = build_avs_context(es,&context);
  if (err) return err;
    
  for (;;)
  {
    avs_frame_p      avs_frame;

    err = get_next_avs_frame(context,TRUE,FALSE,&avs_frame);
    if (err == EOF)
      break;
    else if (err)
    {
      free_avs_context(&context);
      return 1;
    }

    if (avs_frame->is_frame)
    {
      frames ++;
      if (avs_frame->picture_coding_type == AVS_I_PICTURE_CODING)
        printf("i");
      else if (avs_frame->picture_coding_type == AVS_P_PICTURE_CODING)
        printf("p");
      else if (avs_frame->picture_coding_type == AVS_B_PICTURE_CODING)
        printf("b");
      else
        printf("!");
      // Give a *rough* guide as to timing -- assume a constant frame rate
      if (frames % (int)(frame_rate*60) == 0)
        printf("\n%d minute%s\n",frames/(25*60),(frames/(25*60)==1?"":"s"));
    }
    else if (avs_frame->start_code < 0xB0)
      printf("_");                      // slice -- shouldn't happen
    else
    {
      switch (avs_frame->start_code)
      {
      case 0xB0:        // sequence header
        frame_rate = avs_frame_rate(avs_frame->frame_rate_code);
        printf("[");
        break;
      case 0xB1: printf("]"); break;
      case 0xB2: printf("U"); break;
      case 0xB5: printf("E"); break;
      case 0xB7: printf("V"); break;
      default:   /*printf("?");*/ printf("<%x>",avs_frame->start_code); break;
      }
    }

    fflush(stdout);
    count ++;
    free_avs_frame(&avs_frame);

    if (max > 0 && frames >= max)
    {
      printf("\nStopping because %d frames have been read\n",frames);
      break;
    }
  }
  
  printf("\nFound %d frame%s in %d AVS item%s\n",
         frames,(frames==1?"":"s"),count,(count==1?"":"s"));
  free_avs_context(&context);
  return 0;
}

/*
 * Report on data by access unit, as single characters
 *
 * Returns 0 if all went well, 1 if something went wrong.
 */
static int dots_by_access_unit(ES_p  es,
                               int   max,
                               int   verbose,
                               int   hash_eos)
{
  int err = 0;
  int access_unit_count = 0;
  access_unit_context_p  context;

  if (verbose)
    printf("\n"
           "Each character represents a single access unit\n"
           "\n"
           "    D       means an IDR.\n"
           "    d       means an IDR that is not all I slices.\n"
           "    I, P, B means all slices of the primary picture are I, P or B,\n"
           "            and this is a reference picture.\n"
           "    i, p, b means all slices of the primary picture are I, P or B,\n"
           "            and this is NOT a reference picture.\n"
           "    X or x  means that not all slices are of the same type.\n"
           "    ?       means some other type of access unit.\n"
           "    _       means that the access unit doesn't contain a primary picture.\n"
           "\n"
           "If -hasheos was specified:\n"
           "    # means an EOS (end-of-stream) NAL unit.\n"
           "\n");
  
  err = build_access_unit_context(es,&context);
  if (err) return err;
    
  for (;;)
  {
    access_unit_p      access_unit;

    err = get_next_h264_frame(context,TRUE,FALSE,&access_unit);
    if (err == EOF)
      break;
    else if (err)
    {
      free_access_unit_context(&context);
      return 1;
    }

    if (access_unit->primary_start == NULL)
      printf("_");
    else if (access_unit->primary_start->nal_ref_idc == 0)
    {
      if (all_slices_I(access_unit))
        printf("i");
      else if (all_slices_P(access_unit))
        printf("p");
      else if (all_slices_B(access_unit))
        printf("b");
      else
        printf("x");
    }
    else if (access_unit->primary_start->nal_unit_type == NAL_IDR)
    {
      if (all_slices_I(access_unit))
        printf("D");
      else
        printf("d");
    }
    else if (access_unit->primary_start->nal_unit_type == NAL_NON_IDR)
    {
      if (all_slices_I(access_unit))
        printf("I");
      else if (all_slices_P(access_unit))
        printf("P");
      else if (all_slices_B(access_unit))
        printf("B");
      else
        printf("X");
    }
    else
      printf("?");

    fflush(stdout);
    access_unit_count ++;
    free_access_unit(&access_unit);

    // Did the logical stream end after the last access unit?
    if (context->end_of_stream)
    {
      if (hash_eos)
      {
        printf("#");
        // This should be enough to allow us to keep on after the EOS
        context->end_of_stream = FALSE;
        context->no_more_data = FALSE;
      }
      else
      {
        printf("\nStopping because found end-of-stream NAL unit\n");
        break;
      }
    }

    if (max > 0 && context->nac->count >= max)
    {
      printf("\nStopping because %d NAL units have been read\n",
             context->nac->count);
      break;
    }
  }
  
  printf("\nFound %d NAL unit%s in %d access unit%s\n",
         context->nac->count,(context->nac->count==1?"":"s"),
         access_unit_count,(access_unit_count==1?"":"s"));
  free_access_unit_context(&context);
  return 0;
}

/*
 * Simply report on the content of an ES file as single characters for each ES
 * unit
 *
 * - `es` is the input elementary stream
 * - `what_data` should be one of VIDEO_H262, VIDEO_H264 or VIDEO_AVS.
 * - if `max` is non-zero, then reporting will stop after `max` MPEG items
 * - if `verbose` is true, then extra information will be output
 *
 * Returns 0 if it succeeds, 1 if some error occurs.
 */
static int report_file_as_ES_dots(ES_p    es,
                                  int     what_data,
                                  int     max,
                                  int     verbose)
{
  int err = 0;
  int count = 0;
  struct ES_unit unit;

  (void) setup_ES_unit(&unit);

  if (verbose)
  {
    printf("\n"
           "Each character represents a single ES unit\n");
    switch (what_data)
    {
    case VIDEO_H262:
      printf("Pictures are represented according to their picture coding\n"
             "type, and the slices within a picture are not shown.\n"
             "    i means an I picture\n"
             "    p means a  P picture\n"
             "    b means a  B picture\n"
             "    d means a  D picture (these should not occur in MPEG-2)\n"
             "    ! means some other picture (such should not occur)\n"
             "Other items are represented as follows:\n"
             "    [ means a  Sequence header\n"
             "    > means a  Group Start header\n"
             "    E means an Extension start header\n"
             "    U means a  User data header\n"
             "    X means a  Sequence Error\n"
             "    ] means a  Sequence End\n"
             "    R means a  Reserved item\n");
      break;
    case VIDEO_H264:
      printf("### esdots: -es is not yet supported for H.264\n");
      return 1;
      //break;
    case VIDEO_AVS:
      printf("Frames are represented according to their picture coding\n"
             "type, and the slices within a frame are not shown.\n"
             "    i means an I frame\n"
             "    p means a  P frame\n"
             "    b means a  B frame\n"
             "    _ means a slice\n"
             "    ! means something else (this should not be possible)\n"
             "Other items are represented as follows:\n"
             "    [ means a  Sequence header\n"
             "    E means an Extension start header\n"
             "    U means a  User data header\n"
             "    ] means a  Sequence End\n"
             "    V means a  Video edit item\n");
    default:
      printf("### esdots: Unexpected type of data\n");
      return 1;
    }
    printf("    ? means something else. This may indicate that the stream\n"
           "      is not an ES representing AVS (it might, for instance\n"
           "      be PES)\n"
           "\n");
  }
    
  for (;;)
  {
    err = find_next_ES_unit(es,&unit);
    if (err == EOF)
      break;
    else if (err)
      return 1;

    switch (what_data)
    {
    case VIDEO_H262:
      switch (unit.start_code)
      {
        int picture_coding_type;
      case 0x00:
        picture_coding_type = (unit.data[5] & 0x38) >> 3;
        switch (picture_coding_type)
        {
        case 1: printf("i"); break;
        case 2: printf("p"); break;
        case 3: printf("b"); break;
        case 4: printf("d"); break;
        default: printf("!"); break;
        }
        break;
      case 0xB0: printf("R"); break; // Reserved
      case 0xB1: printf("R"); break; // Reserved
      case 0xB2: printf("U"); break; // User data
      case 0xB3: printf("["); break; // SEQUENCE HEADER
      case 0xB4: printf("X"); break; // Sequence error
      case 0xB5: printf("E"); break; // Extension start
      case 0xB6: printf("R"); break; // Reserved
      case 0xB7: printf("]"); break; // SEQUENCE END
      case 0xB8: printf(">"); break; // Group start
      default:
        if (unit.start_code >= 0x01 && unit.start_code <= 0xAF)
          printf("_");
        else
          printf("?");
        break;
      }
      break;
    case VIDEO_H264:
      break;
    case VIDEO_AVS:
      switch (unit.start_code)
      {
      case 0xB3:
        printf("i"); break;
      case 0xB6:
        switch (avs_picture_coding_type(&unit))
        {
        case AVS_P_PICTURE_CODING: printf("p"); break;
        case AVS_B_PICTURE_CODING: printf("b"); break;
        default: printf("!"); break;
        }
        break;
      case 0xB0: printf("["); break;
      case 0xB1: printf("]"); break;
      case 0xB2: printf("U"); break;
      case 0xB5: printf("E"); break;
      case 0xB7: printf("V"); break;
      default:
        if (unit.start_code < 0xB0)
          printf("_");
        else
          printf("?");
        break;
      }
    default: /* shouldn't happen */ break;
    }

    fflush(stdout);
    count ++;

    if (max > 0 && count >= max)
    {
      printf("\nStopping because %d ES units have been read\n",count);
      break;
    }
  }
  clear_ES_unit(&unit);
  
  printf("\nFound %d ES units%s\n",count,(count==1?"":"s"));
  return 0;
}

static void print_usage()
{
  printf(
    "Usage: esdots [switches] [<infile>]\n"
    "\n"
    );
  REPORT_VERSION("esdots");
  printf(
    "\n"
    "  Present the content of an H.264 (MPEG-4/AVC), H.262 (MPEG-2) or AVS\n"
    "  elementary stream as a sequence of characters, representing access\n"
    "  units/MPEG-2 items/AVS items.\n"
    "\n"
    "  (Note that for H.264 it is access units and not frames that are\n"
    "  represented, and for H.262 it is items and not pictures.)\n"
    "\n"
    "Files:\n"
    "  <infile>  is the Elementary Stream file (but see -stdin below)\n"
    "\n"
    "Switches:\n"
    "  -verbose, -v      Preface the output with an explanation of the\n"
    "                    characters being used.\n"
    "  -stdin            Take input from <stdin>, instead of a named file\n"
    "  -max <n>, -m <n>  Maximum number of entities to read\n"
    "  -pes, -ts         The input file is TS or PS, to be read via the\n"
    "                    PES->ES reading mechanisms\n"
    "  -hasheos          Print a # on finding an EOS (end-of-stream) NAL unit\n"
    "                    rather than stopping (only applies to H.264)\n"
    "  -es               Report ES units, rather than any 'higher' unit\n"
    "                    (not necessarily suppported for all file types)\n"
    "\n"
    "Stream type:\n"
    "  If input is from a file, then the program will look at the start of\n"
    "  the file to determine if the stream is H.264 or H.262 data. This\n"
    "  process may occasionally come to the wrong conclusion, in which case\n"
    "  the user can override the choice using the following switches.\n"
    "\n"
    "  For AVS data, the program will never guess correctly, so the user must\n"
    "  specify the file type, using -avs.\n"
    "\n"
    "  If input is from standard input (via -stdin), then it is not possible\n"
    "  for the program to make its own decision on the input stream type.\n"
    "  Instead, it defaults to H.262, and relies on the user indicating if\n"
    "  this is wrong.\n"
    "\n"
    "  -h264, -avc       Force the program to treat the input as MPEG-4/AVC.\n"
    "  -h262             Force the program to treat the input as MPEG-2.\n"
    "  -avs              Force the program to treat the input as AVS.\n"
    );
}

int main(int argc, char **argv)
{
  char  *input_name = NULL;
  int    had_input_name = FALSE;
  int    use_stdin = FALSE;
  int    err = 0;
  ES_p   es = NULL;
  int    max = 0;
  int    verbose = FALSE;
  int    ii = 1;

  int    use_pes = FALSE;
  int    hash_eos = FALSE;

  int     want_data = VIDEO_H262;
  int     is_data = want_data;
  int     force_stream_type = FALSE;

  int     want_ES = FALSE;

  if (argc < 2)
  {
    print_usage();
    return 0;
  }

  while (ii < argc)
  {
    if (argv[ii][0] == '-')
    {
      if (!strcmp("--help",argv[ii]) || !strcmp("-help",argv[ii]) ||
          !strcmp("-h",argv[ii]))
      {
        print_usage();
        return 0;
      }
      else if (!strcmp("-stdin",argv[ii]))
      {
        had_input_name = TRUE; // more or less
        use_stdin = TRUE;
      }
      else if (!strcmp("-avc",argv[ii]) || !strcmp("-h264",argv[ii]))
      {
        force_stream_type = TRUE;
        want_data = VIDEO_H264;
      }
      else if (!strcmp("-h262",argv[ii]))
      {
        force_stream_type = TRUE;
        want_data = VIDEO_H262;
      }
      else if (!strcmp("-avs",argv[ii]))
      {
        force_stream_type = TRUE;
        want_data = VIDEO_AVS;
      }
      else if (!strcmp("-es",argv[ii]))
      {
        want_ES = TRUE;
      }
      else if (!strcmp("-verbose",argv[ii]) || !strcmp("-v",argv[ii]))
      {
        verbose = TRUE;
      }
      else if (!strcmp("-max",argv[ii]) || !strcmp("-m",argv[ii]))
      {
        CHECKARG("es2dots",ii);
        err = int_value("esfilter",argv[ii],argv[ii+1],TRUE,10,&max);
        if (err) return 1;
        ii++;
      }
      else if (!strcmp("-hasheos",argv[ii]))
        hash_eos = TRUE;
      else if (!strcmp("-pes",argv[ii]) || !strcmp("-ts",argv[ii]))
        use_pes = TRUE;
      else
      {
        fprintf(stderr,"### esdots: "
                "Unrecognised command line switch '%s'\n",argv[ii]);
        return 1;
      }
    }
    else
    {
      if (had_input_name)
      {
        fprintf(stderr,"### esdots: Unexpected '%s'\n",argv[ii]);
        return 1;
      }
      else
      {
        input_name = argv[ii];
        had_input_name = TRUE;
      }
    }
    ii++;
  }
  
  if (!had_input_name)
  {
    fprintf(stderr,"### esdots: No input file specified\n");
    return 1;
  }

  err = open_input_as_ES((use_stdin?NULL:input_name),use_pes,FALSE,
                         force_stream_type,want_data,&is_data,&es);
  if (err)
  {
    fprintf(stderr,"### esdots: Error opening input file\n");
    return 1;
  }

  if (want_ES)
    err = report_file_as_ES_dots(es,is_data,max,verbose);
  else if (is_data == VIDEO_H262)
    err = report_h262_file_as_dots(es,max,verbose);
  else if (is_data == VIDEO_H264)
    err = dots_by_access_unit(es,max,verbose,hash_eos);
  else if (is_data == VIDEO_AVS)
    err = report_avs_file_as_dots(es,max,verbose);
  else
  {
    fprintf(stderr,"### esdots: Unexpected type of video data\n");
  }

  if (err)
  {
    fprintf(stderr,"### esdots: Error producing 'dots'\n");
    (void) close_input_as_ES(input_name,&es);
    return 1;
  }

  err = close_input_as_ES(input_name,&es);
  if (err)
  {
    fprintf(stderr,"### esdots: Error closing input file\n");
    return 1;
  }
  return 0;
}