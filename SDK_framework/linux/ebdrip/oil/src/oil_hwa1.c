/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 *
 * $ Repository Name: EBDSDK_P_HWA1/pms/src/src:oil_hwa1.c $
 *
 */
/*! \file
 *  \ingroup OIL
 *  \brief Implementation of the OIL support functions.
 *
 * This file contains the definitions of the functions which initialize and inactivate the OIL
 * and/or individual jobs, as well as various other support functions, including functions
 * for registering resources and RIP modules with the RIP.
 *
 */
#include "oil_interface_oil2pms.h"
#include <string.h>
#include "skinkit.h"
#include "pgbdev.h"
#include "mem.h"
#include "pms_export.h"
#include "oil_platform.h"
#include "oil.h"
#include "oil_interface_skin2oil.h"
#include "oil_job_handler.h"
#include "oil_ebddev.h"
#include "oil_page_handler.h"
#include "oil_malloc.h"
#include "hwa_api.h"
#include "oil_hwa1.h"

/* Force HWA Surface to do one band per buffer */
#define HWA1_SINGLE_BAND_OUTPUT

/* extern variables */
extern OIL_TyConfigurableFeatures g_ConfigurableFeatures;
extern OIL_TyJob *g_pstCurrentJob;
extern OIL_TyPage *g_pstCurrentPage;

static hwa_api_20140721 OIL_hwa1_interface = {
  OIL_HWA1_GetBuffer,
  OIL_HWA1_SubmitBuffer,
  OIL_HWA1_DiscardBuffer,
  OIL_HWA1_GetBands,
  OIL_HWA1_DiscardBands,
  OIL_HWA1_GetOptions
} ;

static hwa_gipa2_lut    oil_hwa_gipa2_cmm;
static hwa_gipa2_lut    oil_hwa_gipa2_bgucr;
static hwa_gipa2_gamma  oil_hwa_gipa2_gamma;
static hwa_gipa2_dither oil_hwa_gipa2_dither;
static hwa_gipa2_user   oil_hwa_gipa2_user;

/* index for the actual FPGA file when required, counts buffers submitted for the current page */
static int page_buffer_index = 0;
/* indicates the last Order List Buffer had no incomplete bands */
static int last_band_complete = 1;
/* id of last complete band submitted */
static int completed_band_id = -1;

/** \brief  Supply the HWA Interface structure to PMS for RDR registration.
*
      \param plength  Will be filled in with the Structure size.

      \return         returns a ponter to the HWA interface structure.
*/
void *OIL_HWA1_GIPA2_Interface(unsigned int *plength)
{
  *plength = sizeof(OIL_hwa1_interface);
  return (&OIL_hwa1_interface);
}


void *OIL_HWA1_GIPA2_Gamma(unsigned int *plength, const unsigned char *Data, const unsigned int Length)
{
  oil_hwa_gipa2_gamma.length = Length;
  oil_hwa_gipa2_gamma.tma = (unsigned char *)Data;
  *plength = sizeof(oil_hwa_gipa2_gamma);
  if (oil_hwa_gipa2_gamma.tma == NULL)
    return NULL;
  else
    return (&oil_hwa_gipa2_gamma);
}

void *OIL_HWA1_GIPA2_BGUCR(unsigned int *plength, const unsigned char *Data, const unsigned int Length)
{
  oil_hwa_gipa2_bgucr.length = Length;
  oil_hwa_gipa2_bgucr.lut = (unsigned int *)Data;
  *plength = sizeof(oil_hwa_gipa2_bgucr);
  if (oil_hwa_gipa2_bgucr.lut == NULL)
    return NULL;
  else
    return (&oil_hwa_gipa2_bgucr);
}

void *OIL_HWA1_GIPA2_CMM(unsigned int *plength, const unsigned char *Data, const unsigned int Length)
{
  oil_hwa_gipa2_cmm.length = Length;
  oil_hwa_gipa2_cmm.lut = (unsigned int *)Data;
  *plength = sizeof(oil_hwa_gipa2_cmm);
  if (oil_hwa_gipa2_cmm.lut == NULL)
    return NULL;
  else
    return (&oil_hwa_gipa2_cmm);
}

void *OIL_HWA1_GIPA2_User(unsigned int *plength, const unsigned char *Data, const unsigned int Length)
{
  oil_hwa_gipa2_user.user = (unsigned char *)Data;
  oil_hwa_gipa2_user.length = Length;
  *plength = sizeof(oil_hwa_gipa2_user);
  if (oil_hwa_gipa2_user.user == NULL)
    return NULL;
  else
    return (&oil_hwa_gipa2_user);
}

void *OIL_HWA1_GIPA2_Dither(unsigned int *plength, const unsigned char **Data, const unsigned int *Length, const unsigned char **CellSize)
{

  if (Data != NULL)
  {
    /* Black Data */
    oil_hwa_gipa2_dither.Kdata = (unsigned char *)Data[0];
    oil_hwa_gipa2_dither.Klength = (int16)Length[0];
    oil_hwa_gipa2_dither.Kwidth = CellSize[0][0];
    oil_hwa_gipa2_dither.Kheight = CellSize[0][1];

    /* Cyan Data */
    oil_hwa_gipa2_dither.Cdata = (unsigned char *)Data[1];
    oil_hwa_gipa2_dither.Clength = (int16)Length[1];
    oil_hwa_gipa2_dither.Cwidth = CellSize[1][0];
    oil_hwa_gipa2_dither.Cheight = CellSize[1][1];

    /* Magenta Data */
    oil_hwa_gipa2_dither.Mdata = (unsigned char *)Data[2];
    oil_hwa_gipa2_dither.Mlength = (int16)Length[2];
    oil_hwa_gipa2_dither.Mwidth = CellSize[2][0];
    oil_hwa_gipa2_dither.Mheight = CellSize[2][1];

    /* Yelow Data */
    oil_hwa_gipa2_dither.Ydata = (unsigned char *)Data[3];
    oil_hwa_gipa2_dither.Ylength = (int16)Length[3];
    oil_hwa_gipa2_dither.Ywidth = CellSize[3][0];
    oil_hwa_gipa2_dither.Yheight = CellSize[3][1];

    *plength = sizeof(oil_hwa_gipa2_dither);
    return (&oil_hwa_gipa2_dither);
  }
  else
  {
    return NULL;
  }
}

  /** \brief  Request a buffer from the skin in which to build an Order List.

      \param page     Timeline reference for the page being rendered (this will
                      be of type SWTLT_RENDERPAGE)

      \param pbuffer  Will be filled in with the buffer address if successful.

      \param plength  Will be filled in with the buffer size if successful.

      \return         HWA_SUCCESS if a buffer is being returned.

                      HWA_ERROR_MEMORY if no buffer is available.

                      HWA_ERROR_SYNTAX if parameters are incorrect.

                      HWA_ERROR_UNKNOWN if the Timeline is unrecognised.

                      HWA_ERROR if there is a persistent hardware failure.

      This is called by the HWA Surface in preparation for starting (or for
      continuing) the rendering of a page via HWA. The hardware support software
      in the skin may return a buffer immediately, block until one becomes
      available, or return an error code if no buffers will become available
      (this is likely to be caused by a programming error).

      Any buffer successfully claimed by this call must subsequently be returned
      by the Surface via the submit_buffer() or discard_buffer() calls below.

      The buffer returned will have 16 byte alignment as required for Order
      Lists, and the Order List must be created at the start of the buffer. The
      Surface may define Assets at the end of the buffer if required.
   */
  hwa_result RIPCALL OIL_HWA1_GetBuffer(sw_tl_ref page, uint8 ** pbuffer, size_t * plength)
  {
    UNUSED_PARAM(sw_tl_ref, page);
    
    /* if buffers already taken and not released/discarded then we need to wait! */
    if (*((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) == 0)
    {
      *pbuffer = g_tSystemInfo.pHWA1_buf1;
      *plength = g_tSystemInfo.cbHWA1_buf1;
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)1;
    }
    else if (*((unsigned char *)g_tSystemInfo.pHWA1_buf2 + PMS_HWA1_BUFFER_SIZE) == 0)
    {
      *pbuffer = g_tSystemInfo.pHWA1_buf2;
      *plength = g_tSystemInfo.cbHWA1_buf2;
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)1;
    }
    else
    {
      return HWA_ERROR_MEMORY;
    }

/*    PMS_HWA1_GetBuffer(pbuffer, plength); */
    return HWA_SUCCESS;
  }

  /** \brief  Submit a completed Order List and Asset buffer.

      \param page           Timeline reference for the page being rendered (of
                            type SWTLT_RENDER_PAGE or SWTLT_RENDER_PARTIAL).

      \param fromband       Identifier of the first band covered by this Order
                            List.

      \param toband         Identifier of the last band covered by the Order
                            List. This is equal to fromband for a single band's
                            Order List.

      \param[in] orderlist  Pointer to the buffer returned from get_buffer(),
                            which contains an HWA Order List.

      \param orderlen       The length of the Order List in the above buffer.

      \param[in] assets     Pointer to the Assets (or Order List Work Data) that
                            the above Order List references. This may be null if
                            the Order List does not use any assets.

      \param assetlen       The length of the Assets in the above buffer, or
                            zero.

      \param complete       This boolean is TRUE if this Order List completes
                            rendering of 'toband' - the final band in the band
                            range. By definition all other bands in the range
                            are complete.

                            It is FALSE if further buffer(s) will be submitted
                            for the final band - in this case those calls MUST
                            start with that band number. In this way bands that
                            cannot be fitted in a single buffer can be continued
                            in another buffer.

                            It is expected that a subsequent call to get_bands()
                            for this incomplete band will receive the same band
                            addresses allocated during the first get_bands()
                            call for that band.

      \return               HWA_SUCCESS if the buffer is submitted successfully.
                            Note that as hardware rendering is asynchronous,
                            this does not indicate that the buffer's contents
                            have or will be successfully processed by hardware.

                            HWA_ERROR_UNKNOWN if the Timeline reference or
                            buffer pointer are not recognised.

                            HWA_ERROR_IN_USE if any of the bands in the given
                            range have already been completed.

                            HWA_ERROR_SYNTAX if any parameters are illegal.

                            HWA_ERROR if there is a persitent hardware failure.

      The Surface may require more than one buffer to represent the contents of
      a page, or even of a single band. Nevertheless, it will submit one buffer
      before requesting another.
    
      Once submitted, the Surface must not attempt to access the contents of the
      buffer.
   */

  /* Number of band address sets allocated. This is incremented by GetBands and
   * decremented by SubmitBuffer and DiscardBands.
   */
  static int hwa_bands = 0 ;

  hwa_result RIPCALL OIL_HWA1_SubmitBuffer(sw_tl_ref page, 
                                      int32 fromband, int32 toband,
                                      uint8 * orderlist, size_t orderlen,
                                      uint8 * assets, size_t assetlen,
                                      HqBool complete)
  {
    UNUSED_PARAM(sw_tl_ref, page);
    /* Check for bands previously completed and return HWA_ERROR_IN_USE */
    if (fromband <= completed_band_id)
      return HWA_ERROR_IN_USE ;
    if (complete)
      completed_band_id = toband;
    else
      completed_band_id = toband - 1;
    /* check validity of parameters and return HWA_ERROR_SYNTAX if invalid */
    page_buffer_index ++;
    PMS_HWA1_SubmitBuffer(orderlist, orderlen, assets, assetlen, last_band_complete, page_buffer_index);

    last_band_complete = complete;

    /* check for a valid buffer address, make it available if submitted */
    /* currently assumes the thread is held by subsequent calls !!!*/
    if ((unsigned char *)g_tSystemInfo.pHWA1_buf1 == orderlist)
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    else if ((unsigned char *)g_tSystemInfo.pHWA1_buf2 == orderlist)
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    else
      return HWA_ERROR_UNKNOWN;
    hwa_bands = 0;
    return HWA_SUCCESS;
  }


  /** \brief  Discard an unused buffer

      \param page       The Timeline reference used in the get_buffer() call
                        that returned this buffer.

      \param[in] buffer The buffer address returned by get_buffer().

      \return           HWA_SUCCESS if the buffer was successfully surrendered.

                        HWA_ERROR_UNKNOWN if the buffer or Timeline reference
                        were not recognised.

      Any buffer returned from get_buffer() MUST subsequently be passed back
      for rendering via submit_buffer() or discarded using this call.
   */
  hwa_result RIPCALL OIL_HWA1_DiscardBuffer(sw_tl_ref page, uint8 * pbuffer) 
  {
    UNUSED_PARAM(sw_tl_ref, page);

    if ((unsigned char *)g_tSystemInfo.pHWA1_buf1 == pbuffer)
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    else if ((unsigned char *)g_tSystemInfo.pHWA1_buf2 == pbuffer)
      *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    else
      return HWA_ERROR_UNKNOWN;
    PMS_HWA1_DiscardBuffer(pbuffer);
    return HWA_SUCCESS;
  }


  /** \brief  Request band buffers from the Skin

      \param page       The Timeline reference for the current page.

      \param[in] buffer The buffer returned from get_buffer() with which this
                        band will be associated.

      \param bandid     Identifier for the band to be rendered.

      \param pbandC     Will be filled in with the band addresses if non-zero.
      \param pbandM
      \param pbandY
      \param pbandK

      \return           HWA_SUCCESS if all requested band buffers are available.

                        HWA_ERROR_MEMORY if there were not enough band buffers.

                        HWA_ERROR_UNKNOWN if the Timeline or buffer is unknown.

                        HWA_ERROR_IN_USE if the band has already been completed.

                        HWA_ERROR_SYNTAX if any parameters are illegal.

                        HWA_ERROR if there is a persistent hardware failure.

      This call allows the Surface to request band buffers to be used by the
      hardware for rendering marks for the band. Once the Surface is unable to
      claim any more band buffers it will submit the Order List it has built
      covering the bands it was able to resource.

      Band buffers claimed by this call cannot be reused until the band they
      are associated with has been fully rendered. The Surface will not request
      bands for colorants that are not used on the page.
   */
  hwa_result RIPCALL OIL_HWA1_GetBands(sw_tl_ref page, uint8 * buffer, int32 bandid,
                                        uint8 ** pbandC, uint8 ** pbandM, uint8 ** pbandY, uint8 ** pbandK)
  {
    UNUSED_PARAM(sw_tl_ref, page);
    UNUSED_PARAM(uint8 *, buffer);

    /* Check for bands previously completed and return HWA_ERROR_IN_USE */
    if (bandid <= completed_band_id)
      return HWA_ERROR_IN_USE ;

#   ifdef HWA1_SINGLE_BAND_OUTPUT
    if (g_tSystemInfo.bHWA_FPGA && hwa_bands > 0)
      return HWA_ERROR_MEMORY ;
#   endif

    switch (PMS_HWA1_GetBands(bandid, pbandC, pbandM, pbandY, pbandK)) {
    case 1:
      break ;
    case 0:
      return HWA_ERROR_MEMORY ;
    case -1:
      return HWA_ERROR_UNKNOWN ;
    default:
      return HWA_ERROR_SYNTAX ;
    }

    ++hwa_bands ;
    return HWA_SUCCESS ;
  }


  /** \brief  Discard previously claimed band buffers not used in an Order List

      \param page       The Timeline reference for the current page.

      \param[in] buffer The order list buffer this band is associated with.

      \param bandid     Identifier for the band.

      \param[in] bandC  The band buffers previously returned from get_band().
      \param[in] bandM
      \param[in] bandY
      \param[in] bandK

      \return           HWA_SUCCESS if the bands are successfully surrendered.

                        HWA_ERROR_UNKNOWN if the Timeline, buffer or and bands
                        are unrecognised.

                        HWA_ERROR_IN_USE if the band is already complete.

                        HWA_ERROR_SYNTAX if any parameters are illegal.

                        HWA_ERROR if there is a persistent hardware failure.
  */
  hwa_result RIPCALL OIL_HWA1_DiscardBands(sw_tl_ref page, uint8 * buffer, int32 bandid,
                                            uint8 * pbandC, uint8 * pbandM, uint8 * pbandY, uint8 * pbandK) 
  {
    UNUSED_PARAM(sw_tl_ref, page);
    UNUSED_PARAM(uint8 *, buffer);

    /* Check for bands previously completed and return HWA_ERROR_IN_USE */
    if (bandid <= completed_band_id)
      return HWA_ERROR_IN_USE ;

    PMS_HWA1_DiscardBands(bandid, pbandC, pbandM, pbandY, pbandK);
    return HWA_SUCCESS;
  }

  hwa_result RIPCALL OIL_HWA1_GetOptions(sw_tl_ref page, hwa_options *values)
  {
    int localoptions = 0;
    
    UNUSED_PARAM(sw_tl_ref, page);
   
    values->bitDepth =  g_tSystemInfo.uHWAOutputDepth;

    if(g_tSystemInfo.bHWA_FPGA)
    {
      values->VCOMAD = PMS_HWA1_FIXED_VCOMAD;
      /* Fixed asset address limits orderlist length too much, so use auto */
      values->VDATAAD = HWA_AUTO_ADDRESS ; /* was PMS_HWA1_FIXED_VDATAAD;*/
      if(PMS_HWA1_ALL_ONE_BAND)
        localoptions |= HWA1_OPT_ALL_ONE_BAND;
      else if(PMS_HWA1_DEFER_BUFFER_ASSETS)
        localoptions |= HWA1_OPT_DEFER_BUFFER_ASSETS;
    } 
    else
    {
      /* Real hardware can do address remapping itself. */
      values->VDATAAD = HWA_NO_ADDRESS;
      values->VCOMAD = HWA_NO_ADDRESS;
    }
#   ifndef HWA_USE_IMEM
      /* FPGA seems to require BAND_DMA regardless of IMEM mode */
      /* localoptions |= HWA1_OPT_NO_IMEM; */
#   endif

    values->options = localoptions;

    return HWA_SUCCESS;
  }
                                            
  /* called from the raster destination callback to set up OIL and PMS page structures?? */
  /* data is used to create file names when the HWA output files are requested           */
  void RIPCALL OIL_HWA1_RasterDescription(RASTER_DESTINATION * pRasterDestination, RasterDescription *pRasterDescription)
  {
  int lineBytes, i;
  UNUSED_PARAM(RASTER_DESTINATION *, pRasterDestination);
    
    if (g_pstCurrentPage == NULL)
    {
      g_pstCurrentPage = CreateOILPage(pRasterDescription);
      g_pstCurrentPage->nBlankPage = FALSE;
      /* Initialise band height etc. so HWA output data and FPGA  header file data can be set up */
      g_pstCurrentPage->atPlane[0].atBand[0].uBandHeight = pRasterDescription->bandHeight;
      g_pstCurrentPage->nRasterWidthData = (((g_pstCurrentPage->nPageWidthPixels * g_tSystemInfo.uHWAOutputDepth) + 63) / 64) * 64;
      lineBytes = g_pstCurrentPage->nRasterWidthData/8;
      for( i = 0; i < OIL_MAX_PLANES_COUNT; i++)
      {
        g_pstCurrentPage->atPlane[i].atBand[0].cbBandSize = lineBytes * pRasterDescription->bandHeight;
        g_pstCurrentPage->atPlane[i].atBand[0].uBandNumber = 0;
        g_pstCurrentPage->atPlane[i].uBandTotal = (pRasterDescription->imageHeight+ pRasterDescription->bandHeight -1)/pRasterDescription->bandHeight;
      }
      /* set to allow the PMS page to copy band setup data */
      /* Set up the actual HWA raster depth definition */
      g_pstCurrentPage->uOutputDepth = g_tSystemInfo.uHWAOutputDepth;
      SubmitPageToPMS();
    }
  }

  /* reset HWA specific data */
  void OIL_HWA1_PageComplete(void)
  {
    page_buffer_index = 0;
    completed_band_id = -1;
    /* reset the buffer in use flags, should always be reset at the end of a page! */
    *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    *((unsigned char *)g_tSystemInfo.pHWA1_buf1 + PMS_HWA1_BUFFER_SIZE) = (unsigned char)0;
    /* this should always be complete at the end of a page! */
    last_band_complete = 1;
    hwa_bands = 0;
  }

