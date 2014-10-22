/** \file
* \ingroup interface
*
* $HopeName: COREinterface_pgb!hwa_api.h(EBDSDK_P.1) $
*
* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
* Global Graphics Software Ltd. Confidential Information.
*
* \brief
* This header file provides details of the HWA API, and defines RDR symbols
* for various reauired tables.
*/


#ifndef __HWA_API_H__
#define __HWA_API_H__

#include "rdrapi.h"
#include "timelineapi.h"

/** \brief HWA_API for sourcing and delivering HWA buffers

    This API is used by Surfaces to source buffers for hardware-specific Order
    Lists and associated Assets and submit the results to the hardware support
    software in the skin. The format of the contents of these buffers is
    implementation dependent and not defined here.

    As of 20140721 the API comprises:
      get_buffer()      Source a free buffer.
      submit_buffer()   Submit an Order List and optionally associated Assets.
      discard_buffer()  Discard a buffer without submitting to the hardware.
      get_band()        Source band buffers.
      discard_band()    Discard band buffers without submitting to the hardware.
      get_options()     Get various configuration settings and addresses.
 */

/* -------------------------------------------------------------------------- */
/* Options
 *
 * The get_options() call returns a structure containing various configuration
 * values, addresses and option flags.
 */

typedef struct {
  int32 VCOMAD ;    /**< Fixed address for orderlist, or HWA_NO_ADDRESS. */
  int32 VDATAAD ;   /**< Fixed address for assetlist, or HWA_NO_ADDRESS. */
  int32 bitDepth ;  /**< HWA Output bit depth - 1, 2 or 4 only. */
  int32 options ;   /**< Configuration options - see below. */
} hwa_options ;

/* Used to indicate that no VCOMAD or VDATAAD remapping is required. */
#define HWA_NO_ADDRESS 0xFFFFFFFFu

/* Used to automatically calculate a VDATAAD address - VCOMAD must be
 * specified. The calculated address will be immediately after the end of the
 * orderlist, rounded up to a multiple of 16 bytes. */
#define HWA_AUTO_ADDRESS 0xFFFFFFFEu

/* Causes renderer bands to be combined into a single orderlist band. */
#define HWA1_OPT_ALL_ONE_BAND        0x2

/* Whether to defer definition of shared assets to the start of the first band.
 * Default behaviour is to use a separate block at the start of the buffer.
 * Overridden by ALL_ONE_BAND above. */
#define HWA1_OPT_DEFER_BUFFER_ASSETS 0x4

/* Set if HWA1 is not operating in IMEM mode. */
#define HWA1_OPT_NO_IMEM             0x8

/* -------------------------------------------------------------------------- */
/** \brief HWA API return values */

typedef int hwa_result ;  /* Return values from the HWA API */

enum {
  HWA_SUCCESS = 0,    /**< Successful call */
  HWA_ERROR,          /**< Some unknown error occurred */
  HWA_ERROR_UNKNOWN,  /**< Unrecognised Timeline, band or buffer reference */
  HWA_ERROR_SYNTAX,   /**< Programming error - illegal parameters */
  HWA_ERROR_IN_USE,   /**< Band has already been rendered */
  HWA_ERROR_MEMORY    /**< No buffers available */
};

/** \brief The HWA_API 20140721 structure.

    This is registered as RDR_API_HWA, defined in apis.h

 */
typedef struct {
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
  hwa_result (RIPCALL *get_buffer)(sw_tl_ref page,
                          /*@in@*/ uint8 ** pbuffer, size_t * plength) ;


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

      All bands allocated by get_bands() for bands in the band range submitted
      to this call are implicitly released by the Surface. An implementation may
      be able to reuse band addresses once they have been rendered and their
      contents imaged, compressed or stored in some way; or it may allocate
      unique band addresses for every band on a page.
   */
  hwa_result (RIPCALL *submit_buffer)(sw_tl_ref page,
                                      int32 fromband, int32 toband,
                             /*@in@*/ uint8 * orderlist, size_t orderlen,
                             /*@in@*/ uint8 * assets, size_t assetlen,
                                      HqBool complete) ;


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
  hwa_result (RIPCALL *discard_buffer)(sw_tl_ref page, uint8 * buffer) ;


  /** \brief  Request band buffers from the Skin

      \param page       The Timeline reference for the current page.

      \param[in] buffer The buffer returned from get_buffer() with which this
                        band will be associated.

      \param bandid     Identifier for the band to be rendered.

      \param pbandC     Will be filled in with the band addresses if non-zero.
      \param pbandM     A null pointer indicates that no band memory is required
      \param pbandY     for that colorant.
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

      If the final band in a buffer is not complete (see submit_buffer() for
      details) then there will be a further call to get_bands() for that band
      number in a subsequent buffer - it is expected that this will return the
      same band addresses as originally allocated to the first call for that
      band number.

      Band addresses allocated by this call are returned either by a successful
      submit_buffer() call for a suitable band range, or by an explicit call to
      discard_bands() in failure cases.
   */
  hwa_result (RIPCALL *get_bands)(sw_tl_ref page,
                         /*@in@*/ uint8 * buffer,
                                  int32 bandid,
              /*@in@*/ /*@null@*/ uint8 ** pbandC,
              /*@in@*/ /*@null@*/ uint8 ** pbandM,
              /*@in@*/ /*@null@*/ uint8 ** pbandY,
              /*@in@*/ /*@null@*/ uint8 ** pbandK) ;


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
  hwa_result (RIPCALL *discard_bands)(sw_tl_ref page,
                             /*@in@*/ uint8 * buffer,
                                      int32 bandid,
                  /*@in@*/ /*@null@*/ uint8 * bandC,
                  /*@in@*/ /*@null@*/ uint8 * bandM,
                  /*@in@*/ /*@null@*/ uint8 * bandY,
                  /*@in@*/ /*@null@*/ uint8 * bandK) ;

  /** \brief  Get configuration options [optional].

      \param page       The Timeline reference for the current page.

      \param[in] config Pointer to an hwa_options structure as described above
                        to be filled in with the required settings for orderlist
                        generation. This structure must have been initialised
                        with default values by the caller, which may be modified
                        by the call.

      \return           HWA_SUCCESS under normal circumstances.

                        HWA_ERROR_UNKNOWN if the Timeline is unrecognised.

                        HWA_ERROR_SYNTAX if any parameters are illegal.

                        HWA_ERROR if there is a persistent hardware failure.
 
      This is an optional method - it is permissible for this API entry to be
      NULL if the default behaviour is sufficient.
  */
 hwa_result (RIPCALL *get_options)(sw_tl_ref page,
                          /*@in@*/ hwa_options * config) ;

} hwa_api_20140721 ;

/* -------------------------------------------------------------------------- */
/** \brief HWA RDR Types

    RDR Types for the RDR_CLASS_HWA Class defined in rdrapi.h.

    In all cases the RDR ID is the bit depth - 1, 2 or 4. It is permissible to
    register the same table for different bit depths.

    In the case of RDR_HWA_GIPA2_TONERLIMIT only the RDR length is used, the ptr
    is ignored. The length gives the actual toner limit in the range 255..1023.
 */

enum {
  RDR_HWA_GIPA2_CMM = 1, /**< The 3D LUT for CMM, in hwa_gipa2_lut format. */
  RDR_HWA_GIPA2_BLACK,   /**< The 3D LUT for BG/UCR, in hwa_gipa2_lut format. */
  RDR_HWA_GIPA2_GAMMA,   /**< The TMA & gamma table in hwa_gipa2_gamma format */
  RDR_HWA_GIPA2_USER,    /**< The user gamma table in hwa_gipa2_user format. */
  RDR_HWA_GIPA2_DITHER,  /**< The dither tables in hwa_gipa2_dither format. */
  RDR_HWA_GIPA2_COLORS,  /**< Dither bit patterns in hwa_gipa2_dither format. */
  RDR_HWA_GIPA2_TONERLIMIT /**< RDR length is the Toner Limit, ptr ignored. */
} ;


/** \brief Format of RDR_HWA_GIPA2_CMM and RDR_HWA_GIPA2_BLACK tables.

    The Black Generation LUT comprises 24 shorts of "K Memory"; 8 unused shorts;
    256 shorts of "WA Memory"; 1024 shorts of "CMYA Memory"; and finally 1024
    shorts of "RGBA Memory", making 4672 bytes in total.

    We have not yet confirmed the format and size of the Color Management LUT.
*/

typedef struct {
  uint32 * lut ;  /**< Pointer to Black LUT or CMM DLUT, 8 byte aligned. */
  size_t length ; /**< Size of the above LUT. */
  uint32 flags ;  /**< Options:
                       b0 clear: LUT must be copied into buffer.
                            set: HWA can address LUT directly. */
} hwa_gipa2_lut ;


/** \brief Format of RDR_HWA_GIPA2_GAMMA table.

    This points to the Toner Mass Adjustment, Reverse Toner Mass Adjustment and
    Gamma tables used by GIPA2.
 */
typedef struct {
  uint8 * tma ;   /**< I_GMMAD actually points to the Cyan, Magenta, Yellow and
                       Black Toner Mass Adjustment 256 byte tables; then the
                       Reverse TMA 256 byte tables for Cyan, Magenta and
                       Yellow; and finally the 256 byte gamma tables for Cyan,
                       Magenta, Yellow and Black - making 2816 bytes in total,
                       all 8 byte aligned. */
  size_t length ; /**< Size of the above tables - should be 2816. */
  uint32 flags ;  /**< Options:
                       b0 clear: Tables must be copied into the buffer.
                            set: HWA can address the tables directly. */
} hwa_gipa2_gamma ;


/** \brief Format of RDR_HWA_GIPA2_USER table.

    This points to the User Gamma tables used by GIPA2.
*/
typedef struct {
  uint8 * user ;  /**< Four 256 byte tables for Cyan, Magenta, Yellow and Black
                       making 1024 bytes in total, 8 byte aligned. */
  size_t length ; /**< Size of the above tables - should be 1024. */
  uint32 flags ;  /**< Options:
                       b0 clear: Tables must be copied into the buffer.
                            set: HWA can address the tables directly. */
} hwa_gipa2_user ;


/** \brief Format of RDR_HWA_GIPA2_DITHER table.

    This information is used by the SET_DITHER_STRETCHBLT command with dither
    threshold data, and the SET_DITHER_SIZE and SET_COLOR commands with dither
    bit pattern data.

    The RDR ID is the bit depth: 1, 2 or 4. So dither cells for 2bit rendering
    are registered as:

    \code
      (void)SwRegisterRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_DITHER, 2,
                          dither2bpp, sizeof(dither2bpp), SW_RDR_NORMAL) ;
    \endcode

    Sizes can only be 16,20,24,32,40,48,56,64 for 1 and 2bit dithers, or
    4,8,12,16,20,24,28,32,40,48 for 4bit dithers.

    It is permissible to only specify the black dither for monochrome devices,
    in which case all CMY fields and pointers must be zero.

    For the RDR_HWA_GIPA2_DITHER case, dither data is a set of thresholds,
    one byte per pixel, packed two pixels to a 32bit word, ie x0,x1,0,0,
    x2,x3,0,0, x4,x5,0,0 etc. Furthermore, each line is padded to 128 bytes
    long, so the total length is Ysize * 128.

    For the RDR_HWA_GIPA2_COLORS case, dither data is a set of 256 bit-pattern
    cells with 1, 2 or 4 bits per pixel, packed into 32bit words with the
    remainder of the last word filled with a repeat of the start of the bit
    pattern.
 */
typedef struct {
  /* Dimensions of the dither cells */
  uint8 Kwidth ;
  uint8 Kheight ;
  uint8 Cwidth ;
  uint8 Cheight ;
  uint8 Mwidth ;
  uint8 Mheight ;
  uint8 Ywidth ;
  uint8 Yheight ;
  /* Pointers to the dither tables */
  uint8 * Kdata ;   /* black dither cell, 8 byte aligned */
  uint8 * Cdata ;   /* cyan dither cell, 8 byte aligned */
  uint8 * Mdata ;   /* magenta dither cell, 8 byte aligned */
  uint8 * Ydata ;   /* yellow dither cell, 8 byte aligned */
  /* Size of the dither tables */
  int16 Klength ;   /* These must be a multiple of 4 or 128 bytes as */
  int16 Clength ;   /* appropriate. */
  int16 Mlength ;
  int16 Ylength ;
  /* Flags */
  uint32 flags ;    /* Options:
                       b0 clear: Dither cells must be copied into buffer.
                            set: HWA can address dither cells directly. */
} hwa_gipa2_dither ;

/** \brief Flags for the above tables */
enum {
  HWA_GIPA2_MUST_COPY = 0,  /* The data must be copied into the buffer. */
  HWA_GIPA2_ACCESSIBLE = 1  /* The data does not need to be in the buffer. */
} ;

/* -------------------------------------------------------------------------- */
#endif /* __HWA_API_H__ */

