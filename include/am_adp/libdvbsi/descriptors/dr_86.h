/*****************************************************************************
 * dr_88.h
 * (c)2001-2008 VideoLAN
 * $Id: dr_58.h 172 2008-04-26 12:10:54Z jpsaman $
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

/*!
 * \file <dr_86.h>
 * \brief Application interface for the caption service
 * descriptor decoder and generator.
 *
 * Application interface for the "caption service" descriptor
 * decoder and generator.
 */

#ifndef _DVBPSI_DR_86_H_
#define _DVBPSI_DR_86_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t language_cod_t[3];

/*****************************************************************************
 * dvbpsi_caption_service_86_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_caption_service_86_s
 * \brief one caption service structure.
 *
 */
/*!
 * \typedef struct dvbpsi_caption_service_86_s dvbpsi_caption_service_86_t
 * \brief dvbpsi_caption_service_86_t type definition.
 */
typedef struct dvbpsi_caption_service_86_s
{
  int                b_digtal_cc;                          /*!< digtal cc flag*/

  /* used if b_digtal_cc is true */
  language_cod_t   iso_639_lang_code;    /*!< language_code */
  uint8_t        i_caption_service_number;  /*!< caption service number */
  int               b_easy_reader;                     /*!< easy reader */
  int               b_wide_aspect_ratio;           /*!< wide aspect ratio */

  /* used if b_digtal_cc is false */
  int               b_line21_field;                      /*!< line21_field */
} dvbpsi_caption_service_86_t;


/*****************************************************************************
 * dvbpsi_caption_service_86_dr_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_caption_service_86_dr_s
 * \brief "caption service" descriptor structure.
 *
 * This structure is used to store a decoded "caption service"
 * descriptor.
 */
/*!
 * \typedef struct dvbpsi_caption_service_86_dr_s dvbpsi_caption_service_86_dr_t
 * \brief dvbpsi_caption_service_86_dr_t type definition.
 */
typedef struct dvbpsi_caption_service_86_dr_s
{
  uint8_t      i_caption_services_number;
  dvbpsi_caption_service_86_t p_caption_service[63];
} dvbpsi_caption_service_86_dr_t;


/*****************************************************************************
 * dvbpsi_DecodeCaptionSerivceDr
 *****************************************************************************/
/*!
 * \fn dvbpsi_caption_service_86_dr_t * dvbpsi_DecodeCaptionService86Dr(
                                        dvbpsi_descriptor_t * p_descriptor)
 * \brief "caption service" descriptor decoder.
 * \param p_descriptor pointer to the descriptor structure
 * \return a pointer to a new "caption service" descriptor structure
 * which contains the decoded data.
 */
dvbpsi_caption_service_86_dr_t* dvbpsi_DecodeCaptionService86Dr(
                                        dvbpsi_descriptor_t * p_descriptor);


/*****************************************************************************
 * dvbpsi_GenCaptionService86Dr
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t * dvbpsi_GenCaptionService86Dr(
                        dvbpsi_caption_service_86_dr_t * p_decoded, int b_duplicate)
 * \brief "caption service" descriptor generator.
 * \param p_decoded pointer to a decoded "caption service" descriptor
 * structure
 * \param b_duplicate if non zero then duplicate the p_decoded structure into
 * the descriptor
 * \return a pointer to a new descriptor structure which contains encoded data.
 */
dvbpsi_descriptor_t * dvbpsi_GenCaptionService86Dr(
                                        dvbpsi_caption_service_86_dr_t * p_decoded,
                                        int b_duplicate);


#ifdef __cplusplus
};
#endif

#else
#error "Multiple inclusions of dr_86.h"
#endif
