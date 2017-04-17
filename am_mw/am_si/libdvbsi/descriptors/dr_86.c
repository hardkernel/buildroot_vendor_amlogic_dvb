#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/*****************************************************************************
 * dr_86.c
 * (c)2001-2008 VideoLAN
 * $Id: dr_58.c 172 2008-04-26 12:10:54Z jpsaman $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *****************************************************************************/


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include "../dvbpsi.h"
#include "../dvbpsi_private.h"
#include "../descriptor.h"

#include "dr_86.h"


/*****************************************************************************
 * dvbpsi_DecodeCaptionService86Dr
 *****************************************************************************/
dvbpsi_caption_service_86_dr_t * dvbpsi_DecodeCaptionService86Dr(
                                        dvbpsi_descriptor_t * p_descriptor)
{
  dvbpsi_caption_service_86_dr_t * p_decoded;
  uint8_t * p_data, * p_end;
  dvbpsi_caption_service_86_t * p_current;

  /* Check the tag */
  if (p_descriptor->i_tag != 0x86)
  {
    DVBPSI_ERROR_ARG("dr_86 decoder", "bad tag (0x%x)", p_descriptor->i_tag);
    return NULL;
  }

  /* Don't decode twice */
  if (p_descriptor->p_decoded)
    return p_descriptor->p_decoded;

  /* Allocate memory */
  p_decoded =
        (dvbpsi_caption_service_86_dr_t*)malloc(sizeof(dvbpsi_caption_service_86_dr_t));
  if (!p_decoded)
  {
    DVBPSI_ERROR("dr_86 decoder", "out of memory");
    return NULL;
  }

  /* Decode data */
  p_decoded->i_caption_services_number = 0;
  p_current = p_decoded->p_caption_service;
  p_end = p_descriptor->p_data + p_descriptor->i_length;
  p_data = p_descriptor->p_data + 1;
  while (p_data + 6 <= p_end) {
    p_current->iso_639_lang_code[0] =   p_data[0];
    p_current->iso_639_lang_code[1] =   p_data[1];
    p_current->iso_639_lang_code[2] =   p_data[2];

    dvbpsi_ToLower(p_current->iso_639_lang_code, 3);

    p_current->b_digtal_cc =   (p_data[3] & 0x80) ? 1 : 0;
    if (p_current->b_digtal_cc)
        p_current->i_caption_service_number =   p_data[3] & 0x3f;
    else
	p_current->b_line21_field = p_data[3] & 0x01;
    p_current->b_easy_reader = (p_data[4] & 0x80) ? 1 : 0;
    p_current->b_wide_aspect_ratio = (p_data[4] & 0x40) ? 1 : 0;
    p_decoded->i_caption_services_number++;
    p_data += 6;
    p_current++;
  }

  p_descriptor->p_decoded = (void*)p_decoded;

  return p_decoded;
}


/*****************************************************************************
 * dvbpsi_GenCaptionService86Dr
 *****************************************************************************/
dvbpsi_descriptor_t * dvbpsi_GenCaptionService86Dr(
                                        dvbpsi_caption_service_86_dr_t * p_decoded,
                                        int b_duplicate)
{
  uint8_t i_num;
  dvbpsi_caption_service_86_t * p_current;
  uint8_t * p_data;

  /* Create the descriptor */
  dvbpsi_descriptor_t * p_descriptor =
        dvbpsi_NewDescriptor(0x86, p_decoded->i_caption_services_number * 6 + 1, NULL);

  if (p_descriptor)
  {
    /* Encode data */

    p_current = p_decoded->p_caption_service;
    p_data = p_descriptor->p_data;

    p_data[0] = (p_decoded->i_caption_services_number & 0x1f) | 0xe0;
    p_data ++;

    for (i_num = 0; i_num < p_decoded->i_caption_services_number; i_num++) {
      p_data[0]  =  p_current->iso_639_lang_code[0];
      p_data[1]  =  p_current->iso_639_lang_code[1];
      p_data[2]  =  p_current->iso_639_lang_code[2];
      if (p_current->b_digtal_cc)
        p_data[3]  =  (p_current->i_caption_service_number & 0x3f) | 0x40 | 0x80;
      else
        p_data[3]  =  (p_current->b_line21_field & 0x01) | 0x3e;
      p_data[4]  =  (p_current->b_easy_reader ? 0x80 : 0) | (p_current->b_wide_aspect_ratio ? 0x40 : 0) | 0x3f;
      p_data[5]  =  0xff;

      p_data += 6;
      p_current++;
    }

    if (b_duplicate)
    {
      /* Duplicate decoded data */
      dvbpsi_caption_service_86_dr_t * p_dup_decoded =
        (dvbpsi_caption_service_86_dr_t*)malloc(sizeof(dvbpsi_caption_service_86_dr_t));
      if (p_dup_decoded)
        memcpy(p_dup_decoded, p_decoded, sizeof(dvbpsi_caption_service_86_dr_t));

      p_descriptor->p_decoded = (void*)p_dup_decoded;
    }
  }

  return p_descriptor;
}
