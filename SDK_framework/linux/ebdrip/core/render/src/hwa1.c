/** \file
 * \ingroup bitblit
 *
 * $HopeName: CORErender!src:hwa1.c(trunk.0) $
 *
 * Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 *
 * \brief
 * RIFT project HWA1 surface definitions.
 */

#include "core.h"
#include "surface.h"
#include "bitblts.h"
#include "bitblth.h" /* blkfillspan */
#include "toneblt.h" /* charbltn */
#include "blttables.h"
#include "halftoneblts.h"
#include "halftoneblks.h"
#include "halftonechar.h"
#include "halftoneimg.h"
#include "render.h"
#include "blitcolors.h"
#include "blitcolorh.h"
#include "pcl5Blit.h"
#include "pclPatternBlit.h"
#include "gu_chan.h"
#include "builtin.h"
#include "renderfn.h"
#include "render.h"
#include "interrupts.h" /* interrupts_clear */
#include "control.h" /* allow_interrupt */
#include "monitor.h" /* monitorf */
#include "spanlist.h" /* spanlist_t */
#include "tables.h" /* highest_bit_set_in_byte */

/* Not keen on exposing all of these here, they should be accessed through
   getter functions on the render_info_t: */
#include "pclAttribTypes.h"
#include "pclAttrib.h"
#include "pclPatternBlit.h"
#include "display.h"

#include "rdrapi.h"
#include "apis.h"
#include "gipa2.h"
#include "hwa_api.h"
#include "swerrors.h"
#include "swcopyf.h"
#include "hqmemcpy.h"
#include "imageo.h"
#include "metrics.h"
#include "rlecache.h"

/* -------------------------------------------------------------------------- */
/* Build options */

/* Define this to cause final band to be the exact remaining number of rows.
 * Leave it undefined to keep all bands the same height. */

#define HWA1_TRUNCATE_FINAL_BAND

/* Define this if color, ROP state etc cannot be carried from band to band.
 * State can never persist from buffer to buffer. */

#define HWA1_BAND_RESETS_STATE

/* Define this to explicitly zero unused fields in GIPA2 commands. */

#define HWA1_ZERO_NOPS

/* Define this to preload all supplied dither cells as a 'buffer asset'.
 * Leave it undefined to add individual dither cells on demand. */

#define HWA1_PRELOAD_CELLS

/* Define this to allow derivation of dither cells from STRETCHBLT thresholds
 * (if available) in the absence of predefined dither cells.
 * Leave it undefined to use a 16x16 Bayer matrix instead. */

#define HWA1_USE_THRESHOLDS

/* Whether to collect and display metrics */

#undef HWA1_METRICS

/* -------------------------------------------------------------------------- */
/* Fix up interactions. */

#ifdef HWA1_ALL_ONE_BAND
  /* All-one-band already does this in a different way. */
# undef HWA1_DEFER_BUFFER_ASSETS
#endif

/* -------------------------------------------------------------------------- */

#undef NOP
#ifdef HWA1_ZERO_NOPS
# define NOP(a) a = 0
#else
# define NOP(a)
#endif

/* -------------------------------------------------------------------------- */
/* Endianness - GIPA2 is BigEndian, so we need accessors for 16bit and 32bit
 * values. */

#ifdef highbytefirst

# define HWA16(s) (s)
# define HWA32(w) (w)
# define PTR32(a) (ptr32)(a)

#else

/* We do these with functions so we can use a function call as a parameter. */

# define HWA16(s) hwa1_hwa16(s)
# define HWA32(w) hwa1_hwa32(w)
# define PTR32(a) hwa1_ptr32(a)

static uint16 hwa1_hwa16(uint16 s)
{
  s = ((255 & s) << 8) | (255 & (s >> 8)) ;

  return s ;
}

static uint32 hwa1_hwa32(uint32 w)
{
  w = ((255 & w) << 24) | ((255 & (w >> 8)) << 16) |
      ((255 & (w >> 16)) << 8) | (255 & (w >> 24)) ;

  return w ;
}

static ptr32 hwa1_ptr32(void * ptr)
{
  uint32 a = (uint32)ptr ;

  a = ((255 & a) << 24) | ((255 & (a >> 8)) << 16) |
      ((255 & (a >> 16)) << 8) | (255 & (a >> 24)) ;

  return (ptr32)a ;
}

#endif

/* -------------------------------------------------------------------------- */

/* Dither cell byte size */
#define HWA1_CELL_SIZE(x,y) \
  ((((((x) * state->config.bitDepth + 31) / 32) * 4 * (y)) + 7) &~7)

/* Threshold matrix to use for cell dither generation as a last resort. */
static uint8 Bayer16x16[] = {
   0, 192,  48, 240,  12, 204,  60, 252,   3, 195,  51, 243,  15, 207,  63, 255,
 128,  64, 176, 112, 140,  76, 188, 124, 131,  67, 179, 115, 143,  79, 191, 127,
  32, 224,  16, 208,  44, 236,  28, 220,  35, 227,  19, 211,  47, 239,  31, 223,
 160,  96, 144,  80, 172, 108, 156,  92, 163,  99, 147,  83, 175, 111, 159,  95,
   8, 200,  56, 248,   4, 196,  52, 244,  11, 203,  59, 251,   7, 199,  55, 247,
 136,  72, 184, 120, 132,  68, 180, 116, 139,  75, 187, 123, 135,  71, 183, 119,
  40, 232,  24, 216,  36, 228,  20, 212,  43, 235,  27, 219,  39, 231,  23, 215,
 168, 104, 152,  88, 164, 100, 148,  84, 171, 107, 155,  91, 167, 103, 151,  87,
   2, 194,  50, 242,  14, 206,  62, 254,   1, 193,  49, 241,  13, 205,  61, 253,
 130,  66, 178, 114, 142,  78, 190, 126, 129,  65, 177, 113, 141,  77, 189, 125,
  34, 226,  18, 210,  46, 238,  30, 222,  33, 225,  17, 209,  45, 237,  29, 221,
 162,  98, 146,  82, 174, 110, 158,  94, 161,  97, 145,  81, 173, 109, 157,  93,
  10, 202,  58, 250,   6, 198,  54, 246,   9, 201,  57, 249,   5, 197,  53, 245,
 138,  74, 186, 122, 134,  70, 182, 118, 137,  73, 185, 121, 133,  69, 181, 117,
  42, 234,  26, 218,  38, 230,  22, 214,  41, 233,  25, 217,  37, 229,  21, 213,
 170, 106, 154,  90, 166, 102, 150,  86, 169, 105, 153,  89, 165, 101, 149,  85
} ;

/* -------------------------------------------------------------------------- */
#ifdef HWA1_METRICS
/* Capture the HWA1 metrics anyway, we may want to print them in non-metrics
  builds. */
typedef struct hwa1_metrics_t {
  size_t total_bytes ;   /**< Total command and asset bytes over all pages. */
  size_t total_cmds ;    /**< Total bytes in commands over all pages. */
  size_t total_assets ;  /**< Total bytes in assets over all pages. */
  size_t total_buffers ; /**< Total buffers output over all pages. */
  size_t peak_band_bytes ;   /**< Max bytes on a band. */
  size_t peak_band_cmds ;    /**< Max bytes in commands on a band. */
  size_t peak_band_assets ;  /**< Max bytes in assets on a band. */
  size_t peak_band_buffers ; /**< Max buffers on a band. */
  size_t this_page_bytes ;   /**< Bytes on current page. */
  size_t this_page_cmds ;    /**< Bytes in commands on current page. */
  size_t this_page_assets ;  /**< Bytes in assets on current page. */
  size_t this_page_buffers ; /**< Buffers on current page. */
  size_t peak_page_bytes ;   /**< Max bytes on a page. */
  size_t peak_page_cmds ;    /**< Max bytes in commands on a page. */
  size_t peak_page_assets ;  /**< Max bytes in assets on a page. */
  size_t peak_page_buffers ; /**< Max buffers on a page. */
  struct {
    uint32 count ;           /**< Number of this opcode */
    size_t cmds ;            /**< Size of commands for opcode. */
    size_t assets ;          /**< Size of assets for opcode. */
  } opcodes[N_GIPA2_OPCODES] ;
} hwa1_metrics_t ;

#define GET_CMD(ptr_, state_, op_) MACRO_START                          \
  ptr_ = (void *)(state_)->ptr ;                                        \
  (state_)->prev = (state_)->ptr ;                                      \
  (state_)->ptr += sizeof(*(ptr_)) ;                                    \
  hwa1_metrics.opcodes[op_].count++ ;                                   \
  hwa1_metrics.opcodes[op_].cmds += sizeof(*(ptr_)) ;                   \
  (ptr_)->opcode = (op_) ;                                              \
  MACRO_END

static hwa1_metrics_t hwa1_metrics ;

static void hwa1_metrics_reset(int reason)
{
  hwa1_metrics_t init = { 0 } ;

  UNUSED_PARAM(int, reason) ;

  hwa1_metrics = init ;
}
#else
#define GET_CMD(ptr_, state_, op_) MACRO_START                          \
  ptr_ = (void *)(state_)->ptr ;                                        \
  (state_)->prev = (state_)->ptr ;                                      \
  (state_)->ptr += sizeof(*(ptr_)) ;                                    \
  (ptr_)->opcode = (op_) ;                                              \
  MACRO_END
#endif

/* -------------------------------------------------------------------------- */

/* Identifiers for assets (debugging purposes ONLY) */

#define ITSA_IMAGE   0x6E8A0100
#define ITSA_CHAR    0xA2C80000
#define ITSA_CLIP    0x19C10000
#define ITSA_CMM     0x880C0000
#define ITSA_BLACK   0xC71A0B00
#define ITSA_DITHER  0xE278D100
#define ITSA_GAMMA   0x8AA80600
#define ITSA_USER    0xE2050000
#define ITSA_CELL    0x11CE0000
#define ITSA_PATTERN 0x287EA709
#define ITSA_DEFAULT 0

/* Trivial list of buffered assets */
typedef struct assetlist {
  struct assetlist * next ; /* Next asset or zero */
  const void *ptr ;         /* Pointer to the SOURCE, the copy follows this */
  int32  arg ;              /* Distinguishing argument */
  int32  padding ;          /* Multiple of 8 bytes */
  /* The asset in question immediately follows this structure */
} assetlist ;


typedef struct hwa_state_t {
  /* HWA1 state */
  uint8   HWA_ROP2 ;        /* The HWA1's current ROP2 */
  uint8   DL_ROP2 ;         /* ROP2 of the next DL object */

  PclDLPattern *HWA_pattern ; /* The pattern set as the HWA1 brush */
  PclDLPattern *DL_pattern ;  /* The PCL pattern to set. */
  Bool HWA_pTrans ;           /* HWA was told the pattern was transparent. */
  Bool DL_pTrans ;            /* DL has transparent pattern. */

  PclPackedColor HWA_color ;  /* The HWA1's current SET_COLOR */
  PclPackedColor DL_color ;   /* Color of the next DL object */

  Bool sTrans ;               /* Source is transparent. */

  dbbox_t bandclip ;          /**< Band clip bounds. */
  dbbox_t rectclip ;          /**< Current HWA1 rect clip. */
  int32 HWA_clipid ;          /**< Current HWA1 complex clip. */
  int32 DL_clipid ;           /**< Top DL complex clip. */
  dbbox_t DL_clipbox ;        /**< Top DL complex clip bbox. */
  int32 DL_clipspans ;        /**< How many spans across the DL clip form? */
  size_t DL_clipsize ;        /**< Clip form size as polygon */

  /* The command buffer */
  uint8 * buffer ;          /* Start of the HWA1 command buffer */
  uint8 * ptr ;             /* New commands go here */
  uint8 * end ;             /* Effective end of the buffer, resources above */
  size_t  length ;          /* Length of the buffer */
  uint8 * prev ;            /* Previous command in buffer */
  assetlist * assets ;      /* List of assets in the buffer (end..length) */
  gipa2_com_head * head ;   /* The most recent COM_HEAD (ie previous band) */

  /* Dither threshold addresses in the current buffer */
  uint8 * Kdither ;
  uint8 * Cdither ;
  uint8 * Mdither ;
  uint8 * Ydither ;

  /* Admin */
  int32 error ;             /* An HWA1 error number, see below */
  sw_tl_ref timeline ;      /* Timeline of the current page */
  int32 firstband ;         /* First band identifier covered by the OrderList */
  int32 lastband ;          /* Last band identifier covered by the OrderList */
  Bool complete ;           /* Whether lastband is complete */
  /* Data required from the setup in PMS/OIL via get options api call */
  hwa_options config ;

  /* Band info */
  int32 bandid ;            /* The band id from the DL state */
  int32 bandheight ;        /* Band height in rows */
  int32 bandwidth ;         /* Band width in pixels (and page width) */
  int32 pageheight ;        /* Height of the whole page */

  OBJECT_NAME_MEMBER
} hwa1_state_t ;

/* HWA1 band and sheet states contain just a single HWA1 state. */
struct surface_band_t {
  hwa1_state_t state ;
} ;

struct surface_sheet_t {
  hwa1_state_t state ;
} ;

enum {
  HWA1_OK = 0,              /* We've all done terribly well */
  HWA1_ERROR_BUFFER,        /* Unable to find an HWA1 buffer */
  HWA1_ERROR_OVERRUN,       /* Tried to put a quart in a pintpot */
  HWA1_ERROR_ROP,           /* Unsupported ROP */
  HWA1_ERROR_CALLBACK       /* Render callback error */
} ;

#define HWA1_BAND_STATE_NAME "HWA1 band state"

/* -------------------------------------------------------------------------- */
/* The HWA API is provided by the skin via RDR. */

static hwa_api_20140721 * hwa_api = NULL ;

/* The various colour management tables are also found through RDR. */
static hwa_gipa2_lut    * hwa_cmm_lut = NULL ;
static hwa_gipa2_lut    * hwa_black_lut = NULL ;
static hwa_gipa2_gamma  * hwa_gamma_table = NULL ;
static hwa_gipa2_user   * hwa_user_table = NULL ;
static hwa_gipa2_dither * hwa_dither = NULL ;
static hwa_gipa2_dither * hwa_colors = NULL ;

/* Toner Limit is actually just a number. */
static uint16 hwa_toner_limit = 1023 ;

/* -------------------------------------------------------------------------- */
/* Asset management
*
* Source forms for STRETCHBLTs and BITBLTs, and scanline arrays for DRAW_RUN
* must be copied into the buffer, but the start of the buffer is a contiguous
* list of command structures.
*
* So we place a simple linked list of such assets at the end of the buffer.
* We identify the original by a single source pointer (eg the form addr) and
* an additional 'arg', so that BITBLTs can be phase-shifted without getting
* confused.
*
* The list is kept in MRU order, so state->asset points to the most recently
* added or found asset, not the lowest in memory. state->end points to the
* lowest asset in effect.
*/

/* Find an asset in the buffer, or return null */
static void *hwa1_find_asset(hwa1_state_t *state, const void *ptr,
  int32 arg)
{
  assetlist * asset, * parent = NULL ;

  for (asset = state->assets;
       asset != NULL;
       parent = asset, asset = asset->next) {
    if (asset->ptr == ptr && asset->arg == arg) {
      if (parent && asset != state->assets) { /* the latter can't be true! */
        parent->next = asset->next ;
        asset->next = state->assets ;
        state->assets = asset ;
      }
      return asset + 1 ;
    }
  }

  return NULL ;
}

static size_t hwa1_asset_size(size_t len)
{
  len = (len + 7) & ~7 ;
  len += sizeof(assetlist) ;

  return len ;
}

/* Reserve space for an asset but don't copy - caller to memcpy afterwards.
*
* This is because BITBLTs will need to be shifted - len must be adjusted
* accordingly by the caller of course.
*/
static void *hwa1_add_asset(hwa1_state_t *state, int32 type,
                            const void *ptr, size_t len, int32 arg)
{
  assetlist * asset ;

  len = hwa1_asset_size(len) ;

  /* Enough room for this in the buffer? There ought to be. */
  if (state->ptr + sizeof(gipa2_com_block_end) > state->end - len)
    return FAILURE(NULL) ;

  /* Make room */
  state->end -= len ;
  asset = (assetlist *)state->end ;

  asset->ptr = ptr ;
  asset->arg = arg ;
  asset->padding = type ;
  asset->next = state->assets ;
  state->assets = asset ;

  return asset + 1 ;
}
/* -------------------------------------------------------------------------- */
/* Return a free HWA1 buffer, stalling if necessary */

static Bool get_hwa1_buffer(hwa1_state_t * state)
{
  hwa_result result ;

  HQASSERT(state, "No state") ;

  /* This may block until a buffer is available. */
  result = hwa_api->get_buffer(state->timeline, &state->buffer, &state->length) ;
  if (result == HWA_SUCCESS)
    return TRUE ;

  switch (result) {
  case HWA_ERROR_MEMORY:
    /* All buffers in use - we would fall back to software rendering if that
     * were possible, but as we can't, we're doomed. */
    HQFAIL("HWA API out of buffers") ;
    break ;
  case HWA_ERROR_UNKNOWN:
    /* The API implementation is unhappy about the Timeline reference. This is
     * likely to indicate a mismatch between what we're trying to render and
     * what it was expecting. */
    HQFAIL("HWA API rejected timeline") ;
    break ;
  case HWA_ERROR:
    /* Hardware error, we're doomed. */
    HQFAIL("HWA API reports hardware fault") ;
    break ;
  case HWA_ERROR_SYNTAX:
    /* We've got something badly wrong. */
    HQFAIL("HWA API syntax error") ;
    break ;
  default:
    HQFAIL("Unanticipated HWA API error") ;
    break ;
  }

  return FALSE ;
}

/* Unique identifiers for asset management */
uint8 cache_id[8] ;
#define DITHER_CACHE (&cache_id[0])
#define CLIP_ASSET   (&cache_id[1])
#define ONE_OFF      (&cache_id[2])
#define CELL_C       (&cache_id[3])
#define CELL_M       (&cache_id[4])
#define CELL_Y       (&cache_id[5])
#define CELL_K       (&cache_id[6])

/* Set up the state with a new buffer, potentially initialised with any assets
   or setup commands shared between bands in the buffer.
*/

static Bool hwa1_buffer_assets(hwa1_state_t * state, Bool separate)
{
  gipa2_com_head * head ;
  gipa2_set_dither_size * size ;
  gipa2_set_dither_stretchblt * dither ;

  if (separate) {
    /* First the COM_HEAD - this is a bandless command block */
    GET_CMD(head, state, GIPA2_COM_HEAD) ;
    NOP(head->nop1) ;
    NOP(head->nop2) ;
    NOP(head->nop3) ;
    head->next = 0 ;
    NOP(head->nop4) ;
    NOP(head->nop5) ;
    state->head = head ;
  }

  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) != 0) {
    int bitdepth = state->config.bitDepth ;
    gipa2_band_init * bandinit ;
    uint8 * C, *M, *Y, *K ;
    hwa_result result ;

    result = hwa_api->get_bands(state->timeline, state->buffer, 0,
      &C, &M, &Y, &K) ;

    GET_CMD(bandinit, state, GIPA2_BAND_INIT) ;
    NOP(bandinit->nop1) ;
    NOP(bandinit->nop2) ;
    NOP(bandinit->nop3) ;

    /* It was a lot easier when the Skin had to initialise these... */
    bandinit->planes = GIPA2_CMYK ;
    bandinit->bitdepth = (bitdepth == 1) ? GIPA2_1BIT :
      (bitdepth == 2) ? GIPA2_2BIT : GIPA2_4BIT ;

    bandinit->pixels = HWA32(state->bandwidth) ;
    bandinit->bytes = HWA32(((state->bandwidth * bitdepth + 63) / 64) * 8) ;
    bandinit->rows = HWA32(state->pageheight) ;
    bandinit->offset = HWA32(0) ;

    /* Page width and page height aren't used, but may be useful for debug */
    bandinit->nop2 = HWA32(state->bandwidth) ;
    bandinit->nop3 = HWA32(state->pageheight) ;

    /* The band addresses from the Skin */
    bandinit->kband = PTR32(K) ;
    bandinit->cband = PTR32(C) ;
    bandinit->mband = PTR32(M) ;
    bandinit->yband = PTR32(Y) ;
  }

  /* Define the SET_DITHER_STRETCHBLT. If this table isn't present, define a
   * tiny all-black one.

   * The GT and 255 flags are as per RIFT's non-trivial example files, but
   * we use the no-clip form because we want to set this once and reuse it for
   * all STRETCHBLTs. Rift were unable to clarify whether there would be any
   * effect on performance from this strategy, though their examples issue a
   * SET_DITHER and SET_GAMMA before every DRAW_STRETCHBLT.
   */
  GET_CMD(dither, state, GIPA2_SET_DITHER_STRETCHBLT) ;
  NOP(dither->nop1) ;
  NOP(dither->nop2) ;
  NOP(dither->ksize) ;
  NOP(dither->csize) ;
  NOP(dither->msize) ;
  NOP(dither->ysize) ;
  dither->flags = GIPA2_DITHER_GT | GIPA2_DITHER_255 | GIPA2_DITHER_NOCLIP ;

  if (hwa_dither && hwa_dither->Klength) {
    /* Dither patterns are defined, though we may not have to define them as
     * an asset.
     */
    dither->kwidth  = hwa_dither->Kwidth ;
    dither->kheight = hwa_dither->Kheight ;
    dither->cwidth  = hwa_dither->Cwidth ;
    dither->cheight = hwa_dither->Cheight ;
    dither->mwidth  = hwa_dither->Mwidth ;
    dither->mheight = hwa_dither->Mheight ;
    dither->ywidth  = hwa_dither->Ywidth ;
    dither->yheight = hwa_dither->Yheight ;

    if ((hwa_dither->flags & HWA_GIPA2_ACCESSIBLE) == 0) {
      /* We must define the dithers as an asset */
      uint8 * K, *C, *M, *Y ;
      size_t length = hwa_dither->Klength + hwa_dither->Clength +
                      hwa_dither->Mlength + hwa_dither->Ylength ;

      /* Need to copy the dithers into the buffer as an asset */
      K = hwa1_add_asset(state, ITSA_DITHER, DITHER_CACHE, length, 0) ;
      if (!K)
        return FALSE ;
      C = K + hwa_dither->Klength ;
      M = C + hwa_dither->Clength ;
      Y = M + hwa_dither->Mlength ;
      dither->kcell = PTR32(K) ;
      dither->ccell = PTR32(C) ;
      dither->mcell = PTR32(M) ;
      dither->ycell = PTR32(Y) ;
      HqMemCpy(K, hwa_dither->Kdata, hwa_dither->Klength) ;
      HqMemCpy(C, hwa_dither->Cdata, hwa_dither->Clength) ;
      HqMemCpy(M, hwa_dither->Mdata, hwa_dither->Mlength) ;
      HqMemCpy(Y, hwa_dither->Ydata, hwa_dither->Ylength) ;

    } else {
      /* Dithers are directly addressable
       *
       * Note that we haven't confirmed yet HOW these addresses are set for the
       * IMEM case - we know that the hardware does VMEM to IMEM address
       * remapping, as the OL and Assets won't be at the same address in its
       * IMEM as they were in our (VMEM) memory... so all absolute addresses in
       * the OL will be "wrong".
       *
       * However, for persistent data in IMEM we don't know whether those
       * addresses would be subject to the same address remapping:
       * Next COM_HEAD = COM_HEAD.next - VCOMAD + ICOMAD
       * Required asset = <command ptr> - VDATAAD + IDATAAD
       *
       * Or whether such remapping is delimited not by context (as above) but
       * by address range. ie:
       * Address in range VCOMAD to VCOMAD + OLSIZE: += ICOMAD - VCOMAD
       * Address in range VDATAAD to VDATAAD + DATASIZE: += IDATAAD - VDATAAD
       *
       * This affects what address would be put in the SET_DITHER_STRETCHBLT
       * command - the absolute IMEM address, or a faked VMEM address that
       * will end up pointing at the right bit of IMEM after address remapping?
       * If the latter, it would depend upon VDATAAD - the start of our assets
       * - and as such won't be known until we stop adding assets and submit
       * the buffer.
       *
       * We can't know the correct address at the point we create the command,
       * so a relocation pass would be required during submit, but only for
       * those addresses marked by HWA_GIPA2_ACCESSIBLE. Fiddly, but not rocket
       * science.
       *
       * This surface though does not know if we're in IMEM mode... implying
       * the Hardware Support Software would have to do that address fixup if
       * it has chosen to use the HWA_GIPA2_ACCESSIBLE flag in IMEM mode.
       */

      /* These macros mark such absolute addresses - GIPA2_PTR32() is used to
       * write into a command structure, GIPA2_ADDR() to assign to a variable
       * that will later be written into a structure using PTR32().
       *
       * These may have to be offset, possibly when the buffer is submitted.
       */
#     define GIPA2_ADDR(a,v) a = (v)
#     define GIPA2_PTR32(a,v) a = PTR32(v)

      GIPA2_PTR32(dither->kcell, hwa_dither->Kdata) ;
      GIPA2_PTR32(dither->ccell, hwa_dither->Cdata) ;
      GIPA2_PTR32(dither->mcell, hwa_dither->Mdata) ;
      GIPA2_PTR32(dither->ycell, hwa_dither->Ydata) ;
    }
  } else {
    uint8 * cell ;
    int x, y ;
    /* No dithers supplied, fake one for testing purposes */
    dither->kwidth  = 16 ;
    dither->kheight = 16 ;
    dither->cwidth  = 16 ;
    dither->cheight = 16 ;
    dither->mwidth  = 16 ;
    dither->mheight = 16 ;
    dither->ywidth  = 16 ;
    dither->yheight = 16 ;

    cell = hwa1_add_asset(state, ITSA_DITHER, DITHER_CACHE, 16 * 128, 0) ;
    dither->kcell = dither->ccell = dither->mcell = dither->ycell = PTR32(cell) ;
    HqMemSet8(cell, 0, 16 * 128) ;
    /* This is a very coarse horizontal line screen */
    for (y = 0 ; y < 16 ; ++y) {
      uint8 * row = cell + y * 128 ;
      for (x = 0 ; x < 16 ; x += 2) {
        row[x * 2 + 0] = (uint8)((y * 16) + x) ;
        row[x * 2 + 1] = (uint8)((y * 16) + x + 1) ;
      }
    }
  }

  /* And the dither size, for SET_COLOR's benefit */
  state->Kdither = state->Cdither = state->Mdither = state->Ydither = NULL ;
  GET_CMD(size, state, GIPA2_SET_DITHER_SIZE) ;
  NOP(size->nop1) ;
  NOP(size->nop2) ;
  NOP(size->nop3) ;
  if (hwa_colors && hwa_colors->Kdata) {
    size->kwidth  = hwa_colors->Kwidth ;
    size->kheight = hwa_colors->Kheight ;
    size->cwidth  = hwa_colors->Cwidth ;
    size->cheight = hwa_colors->Cheight ;
    size->mwidth  = hwa_colors->Mwidth ;
    size->mheight = hwa_colors->Mheight ;
    size->ywidth  = hwa_colors->Ywidth ;
    size->yheight = hwa_colors->Yheight ;

    /* Now the dither data itself */
    if ((hwa_colors->flags & HWA_GIPA2_ACCESSIBLE) != 0) {
      /* GIPA2 already has the data */
      GIPA2_PTR32(state->Kdither, hwa_colors->Kdata) ;
      GIPA2_PTR32(state->Cdither, hwa_colors->Cdata) ;
      GIPA2_PTR32(state->Mdither, hwa_colors->Mdata) ;
      GIPA2_PTR32(state->Ydither, hwa_colors->Ydata) ;
    }
#ifdef HWA1_PRELOAD_CELLS
    else {
      /* Must define the dither cells as an asset */
      uint8 * K, *C, *M, *Y ;
      size_t len = hwa_colors->Klength + hwa_colors->Clength +
        hwa_colors->Mlength + hwa_colors->Ylength ;
      K = hwa1_add_asset(state, ITSA_DITHER, DITHER_CACHE, len, 1) ;
      C = K + hwa_colors->Klength ;
      M = C + hwa_colors->Clength ;
      Y = M + hwa_colors->Mlength ;
      state->Kdither = PTR32(K) ;
      state->Cdither = PTR32(C) ;
      state->Mdither = PTR32(M) ;
      state->Ydither = PTR32(Y) ;
      HqMemCpy(K, hwa_colors->Kdata, hwa_colors->Klength) ;
      HqMemCpy(C, hwa_colors->Cdata, hwa_colors->Clength) ;
      HqMemCpy(M, hwa_colors->Mdata, hwa_colors->Mlength) ;
      HqMemCpy(Y, hwa_colors->Ydata, hwa_colors->Ylength) ;
    }
#endif

#ifdef HWA1_USE_THRESHOLDS
  } else if (hwa_dither) {
    size->kwidth  = hwa_dither->Kwidth ;
    size->kheight = hwa_dither->Kheight ;
    size->cwidth  = hwa_dither->Cwidth ;
    size->cheight = hwa_dither->Cheight ;
    size->mwidth  = hwa_dither->Mwidth ;
    size->mheight = hwa_dither->Mheight ;
    size->ywidth  = hwa_dither->Ywidth ;
    size->yheight = hwa_dither->Yheight ;
#endif

  } else {
    /* In the absence of a definition, fake dithers on the fly. */
    size->kwidth  = 16 ;
    size->kheight = 16 ;
    size->cwidth  = 16 ;
    size->cheight = 16 ;
    size->mwidth  = 16 ;
    size->mheight = 16 ;
    size->ywidth  = 16 ;
    size->yheight = 16 ;
  }


  /* The CMM tables via SET_GAMMA_STRETCHBLT */
  if (hwa_gamma_table || hwa_black_lut || hwa_cmm_lut) {
    gipa2_set_gamma_stretchblt * cmm ;
    uint32 * cmm_lut = NULL ;
    uint32 * black_lut = NULL ;
    uint8 * gamma_tables = NULL ;
    uint8 flags = GIPA2_G2K ;

    if (hwa_toner_limit < 255 * 4)
      flags |= GIPA2_TONERLIMIT ;

    if (hwa_gamma_table) {
      flags |= GIPA2_GAMMA | GIPA2_TONERMASS ;
      if ((hwa_gamma_table->flags & HWA_GIPA2_ACCESSIBLE) != 0) {
        /* Gamma is directly addressable. */
        GIPA2_ADDR(gamma_tables, hwa_gamma_table->tma) ;
      } else {
        /* Copy the black LUT as an asset */
        gamma_tables = hwa1_add_asset(state, ITSA_GAMMA,
                                      ONE_OFF, hwa_gamma_table->length, 0) ;
        if (!gamma_tables)
          return FALSE ;
        HqMemCpy(gamma_tables, hwa_gamma_table->tma, hwa_gamma_table->length) ;
      }
    }

    if (hwa_black_lut) {
      flags |= GIPA2_BGUCR ;
      if ((hwa_black_lut->flags & HWA_GIPA2_ACCESSIBLE) != 0) {
        /* LUT is directly addressable. */
        GIPA2_ADDR(black_lut, hwa_black_lut->lut) ;
      } else {
        /* Copy the black LUT as an asset */
        black_lut = hwa1_add_asset(state, ITSA_BLACK,
                                   ONE_OFF, hwa_black_lut->length, 1) ;
        if (!black_lut)
          return FALSE ;
        HqMemCpy(black_lut, hwa_black_lut->lut, hwa_black_lut->length) ;
      }
    }

    if (hwa_cmm_lut) {
      flags |= GIPA2_CMM | GIPA2_PRECMMGRAY ;
      if ((hwa_cmm_lut->flags & HWA_GIPA2_ACCESSIBLE) != 0) {
        /* LUT is directly addressable. */
        GIPA2_ADDR(cmm_lut, hwa_cmm_lut->lut) ;
      } else {
        /* Copy the CMM LUT as an asset */
        cmm_lut = hwa1_add_asset(state, ITSA_CMM,
                                 ONE_OFF, hwa_cmm_lut->length, 2) ;
        if (!cmm_lut)
          return FALSE ;
        HqMemCpy(cmm_lut, hwa_cmm_lut->lut, hwa_cmm_lut->length) ;
      }
    }

    GET_CMD(cmm, state, GIPA2_SET_GAMMA_STRETCHBLT) ;
    cmm->flags = flags ;
    cmm->tonerlimit = HWA16(hwa_toner_limit) ;
    cmm->gamma = PTR32(gamma_tables) ;
    cmm->black = PTR32(black_lut) ;
    cmm->cmm = PTR32(cmm_lut) ;
  }

  /* The user gamma table if supplied */
  if (hwa_user_table) {
    gipa2_set_usergamma_stretchblt * user ;
    uint8 * user_gamma ;

    if ((hwa_user_table->flags & HWA_GIPA2_ACCESSIBLE) != 0) {
      /* User gamma is directly addressable. */
      GIPA2_ADDR(user_gamma, hwa_user_table->user) ;
    } else {
      /* Copy the user gamma as an asset */
      user_gamma = hwa1_add_asset(state, ITSA_USER,
                                  ONE_OFF, hwa_user_table->length, 3) ;
      if (!user_gamma)
        return FALSE ;
      HqMemCpy(user_gamma, hwa_user_table->user, hwa_user_table->length) ;
    }

    GET_CMD(user, state, GIPA2_SET_USERGAMMA_STRETCHBLT) ;
    user->flags = GIPA2_USERGAMMA ;
    NOP(user->nop) ;
    user->user = PTR32(user_gamma) ;
  }

  if (separate &&
      (state->config.options & HWA1_OPT_ALL_ONE_BAND) == 0) {
    /* Finally the COM_BLOCK_END for this tiny bandless block */
    gipa2_com_block_end * end ;

    GET_CMD(end, state, GIPA2_COM_BLOCK_END) ;
    NOP(end->nop1) ;
    NOP(end->nop2) ;
    NOP(end->nop3) ;
  }

  return TRUE ;
}


static Bool hwa1_new_buffer(hwa1_state_t *state)
{
  /* Get the buffer, stalling if necessary */
  if (!get_hwa1_buffer(state)) {
    state->error = HWA1_ERROR_BUFFER ;
    return FAILURE(FALSE) ;
  }

  /* initialise our pointers etc */
  state->ptr    = state->buffer ;
  state->end    = state->buffer + state->length ;
  state->assets = NULL ;
  state->head   = NULL ;    /* We don't do COM_HEAD until band_start */
  state->prev   = NULL ;

  /* Now the shared assets and setup commands... */
  if ((state->config.options & HWA1_OPT_DEFER_BUFFER_ASSETS) == 0) {
    /* Buffer assets not defered */
    return hwa1_buffer_assets(state, TRUE) ;
  }

  return TRUE ;
}


static Bool hwa1_band_end(hwa1_state_t * state, Bool complete)
{
  gipa2_band_dma * banddma ;
  gipa2_com_block_end * end ;

  /* We don't check if BAND_DMA command will fit, because it is always
  * allowed for in size comparisons. The /next/ wont_fit() may flush
  * the buffer though.
  */

  /* We're not supposed to include this if there's no IMEM. */
  if ((state->config.options & HWA1_OPT_NO_IMEM) == 0) {
    GET_CMD(banddma, state, GIPA2_BAND_DMA) ;
    NOP(banddma->nop1) ;
    NOP(banddma->nop2) ;
    NOP(banddma->nop3) ;
  }

  /* These two always have to go together, sadly. You'd think you could do
   * multiple BAND_INIT/BAND_DMA within a single COM_HEAD/COM_BLOCK_END, but no.
   */
  GET_CMD(end, state, GIPA2_COM_BLOCK_END) ;
  NOP(end->nop1) ;
  NOP(end->nop2) ;
  NOP(end->nop3) ;

  state->complete = complete ;

  return TRUE ;
}


/* Address remapping, if required */

/* Remap a pointer into the assets */
static void hwa1_data_ad(hwa1_state_t * state, void * vptr)
{
  if (state->config.VDATAAD != HWA_NO_ADDRESS) {
    uint32 * ptr = vptr ;
    uint32 addr = HWA32(*ptr) ;
    if (addr) {
      addr -= (uint32)state->end ;
      addr += state->config.VDATAAD ;
      *ptr = HWA32(addr) ;
    }
  }
}

/* Remap a pointer into the orderlist - COM_HEAD:next only */
static void hwa1_com_ad(hwa1_state_t * state, void * vptr)
{
  if (state->config.VCOMAD != HWA_NO_ADDRESS) {
    uint32 * ptr = vptr ;
    uint32 addr = HWA32(*ptr) ;
    if (addr) {
      addr -= (uint32)state->buffer ;
      addr += state->config.VCOMAD ;
      *ptr = HWA32(addr) ;
    }
  }
}

static void hwa1_remap_addresses(hwa1_state_t * state)
{
  Bool auto_addr = (state->config.VDATAAD == HWA_AUTO_ADDRESS) ;
  uint8 * ptr = state->buffer, *next = NULL ;

  if (auto_addr)
    state->config.VDATAAD = state->config.VCOMAD +
                            ((state->ptr - state->buffer + 0xFFF) &~0xFFF) ;

  do {
    uint8 op = ptr[0] ;
    int size = 4 ;

    switch (op) {
    case GIPA2_COM_HEAD:
    {
      gipa2_com_head * head = (gipa2_com_head *) ptr ;
      size = sizeof(gipa2_com_head) ;

      next = PTR32(head->next) ;
      hwa1_com_ad(state, &head->next) ;

      break ;
    }

    case GIPA2_PAGE_INIT:
      size = sizeof(gipa2_page_init) ;
      break ;

    case GIPA2_BAND_DMA:
      size = sizeof(gipa2_band_dma) ;
      break ;

    case GIPA2_BAND_INIT:
      size = sizeof(gipa2_band_init) ;
      break ;

    case GIPA2_DRAW_RUN_REPEAT:
    {
      gipa2_draw_run_repeat_2 * repeat = (gipa2_draw_run_repeat_2 *) ptr ;

      if (repeat->coords == GIPA2_COORDS_ARE_2)
        size = sizeof(gipa2_draw_run_repeat_2) ;
      else
        size = sizeof(gipa2_draw_run_repeat_4) ;

      break ;
    }

    case GIPA2_DRAW_RUN:
    {
      gipa2_draw_run * run = (gipa2_draw_run *) ptr ;

      if ((run->flags & GIPA2_YCLIP) != 0)
        size = sizeof(gipa2_draw_run_yclip) ;
      else
        size = sizeof(gipa2_draw_run) ;

      hwa1_data_ad(state, &run->scanlines) ;
      break ;
    }

    case GIPA2_DRAW_BITBLT:
    {
      gipa2_draw_bitblt * bitblt = (gipa2_draw_bitblt *) ptr ;
      size = sizeof(gipa2_draw_bitblt) ;

      hwa1_data_ad(state, &bitblt->data) ;

      break ;
    }

    case GIPA2_DRAW_STRETCHBLT:
    {
      gipa2_draw_stretchblt * stretch = (gipa2_draw_stretchblt *) ptr ;
      size = sizeof(gipa2_draw_stretchblt) ;

      hwa1_data_ad(state, &stretch->data) ;

      break ;
    }

    case GIPA2_SET_ROP2:
      size = sizeof(gipa2_set_rop2) ;
      break ;

    case GIPA2_SET_COLOR:
    {
      gipa2_set_color_bg * color = (gipa2_set_color_bg *) ptr ;
      if (color->bg)
        size = sizeof(gipa2_set_color_bg) ;
      else
        size = sizeof(gipa2_set_color) ;

      hwa1_data_ad(state, &color->cfore) ;
      hwa1_data_ad(state, &color->mfore) ;
      hwa1_data_ad(state, &color->yfore) ;
      hwa1_data_ad(state, &color->kfore) ;
      if (color->bg) {
        hwa1_data_ad(state, &color->cback) ;
        hwa1_data_ad(state, &color->mback) ;
        hwa1_data_ad(state, &color->yback) ;
        hwa1_data_ad(state, &color->kback) ;
      }
      break ;
    }

    case GIPA2_SET_DITHER_STRETCHBLT:
    {
      gipa2_set_dither_stretchblt * dither ;
      dither = (gipa2_set_dither_stretchblt *) ptr ;
      if ((dither->flags & GIPA2_YCLIP) != 0)
        size = sizeof(gipa2_set_dither_stretchblt_yclip) ;
      else
        size = sizeof(gipa2_set_dither_stretchblt) ;

      hwa1_data_ad(state, &dither->ccell) ;
      hwa1_data_ad(state, &dither->mcell) ;
      hwa1_data_ad(state, &dither->ycell) ;
      hwa1_data_ad(state, &dither->kcell) ;

      break ;
    }

    case GIPA2_SET_GAMMA_STRETCHBLT:
    case GIPA2_SET_GAMMA_STRETCHBLT2:
    {
      gipa2_set_gamma_stretchblt * gamma ;
      gamma = (gipa2_set_gamma_stretchblt *) ptr ;
      if (op == GIPA2_SET_GAMMA_STRETCHBLT)
        size = sizeof(gipa2_set_gamma_stretchblt) ;
      else
        size = sizeof(gipa2_set_gamma_stretchblt2) ;

      hwa1_data_ad(state, &gamma->gamma) ;
      hwa1_data_ad(state, &gamma->black) ;
      hwa1_data_ad(state, &gamma->cmm) ;

      break ;
    }

    case GIPA2_SET_DITHER_SIZE:
      size = sizeof(gipa2_set_dither_size) ;
      break ;

    case GIPA2_SET_CLIP:
    {
      gipa2_set_clip_2 * clip = (gipa2_set_clip_2 *) ptr ;
      if ((clip->flags & GIPA2_COORDS_ARE_4) != 0) {
        gipa2_set_clip_4 * clip = (gipa2_set_clip_4 *) ptr ;
        size = sizeof(gipa2_set_clip_4) ;

        hwa1_data_ad(state, &clip->data) ;
      } else {
        size = sizeof(gipa2_set_clip_2) ;

        hwa1_data_ad(state, &clip->data) ;
      }
      break ;
    }

    case GIPA2_RESET_CLIP:
      size = sizeof(gipa2_reset_clip) ;
      break ;

    case GIPA2_SET_INDEXCOLOR:
      size = sizeof(gipa2_set_indexcolor) ;
      break ;

    case GIPA2_SET_PENETRATIONCOLOR:
      size = sizeof(gipa2_set_penetration_color) ;
      break ;

    case GIPA2_SET_USERGAMMA_STRETCHBLT:
    {
      gipa2_set_usergamma_stretchblt * user ;
      user = (gipa2_set_usergamma_stretchblt *) ptr ;
      if ((user->flags & GIPA2_YCLIP) != 0)
        size = sizeof(gipa2_set_usergamma_stretchblt_yclip) ;
      else
        size = sizeof(gipa2_set_usergamma_stretchblt) ;

      hwa1_data_ad(state, &user->user) ;

      break ;
    }

    case GIPA2_SET_BRUSH:
    {
      gipa2_set_brush * brush = (gipa2_set_brush *) ptr ;
      size = sizeof(gipa2_set_brush) ;

      hwa1_data_ad(state, &brush->data) ;

      break ;
    }

    case GIPA2_COM_BLOCK_END:
      ptr = next ;
      size = 0 ;
      next = NULL ;
      break ;

    default:
      HQFAIL("Unexpected command in orderlist during address remapping") ;
      ptr = NULL ;
      size = 0 ;
      break ;
    }
    ptr += size ;
  } while (ptr) ;

  if (auto_addr)
    state->config.VDATAAD = HWA_AUTO_ADDRESS ;
}



static Bool hwa1_submit_buffer(hwa1_state_t *state, Bool complete)
{
  size_t cmds, assets ;
  hwa_result result ;

  HQASSERT(complete == state->complete,
           "HWA1 confused about band completion.") ;

  if (!complete)
    hwa1_band_end(state, FALSE) ;

  /* Command space used. */
  cmds = state->ptr - state->buffer ;
  /* Asset space used. */
  assets = (state->assets) == 0 ? 0 :
    state->buffer + state->length - (uint8*)state->end ;

  if (state->config.VDATAAD != HWA_NO_ADDRESS ||
      state->config.VCOMAD  != HWA_NO_ADDRESS)
    hwa1_remap_addresses(state) ;

  /* Pass the buffer to the HWA1 */
  result = hwa_api->submit_buffer(state->timeline,
                                  state->firstband, state->lastband,
                                  state->buffer, cmds,
                                  state->end, assets,
                                  complete) ;


# ifdef HWA1_METRICS
  /* Note that we've used another buffer. */
  hwa1_metrics.this_page_buffers += 1 ;

  hwa1_metrics.this_page_cmds += cmds ;
  hwa1_metrics.this_page_assets += assets ;
  hwa1_metrics.this_page_bytes += cmds + assets ;
# endif

  /* If complete, make no assumption about next band to be processed. */
  if (complete) {
    state->firstband = state->lastband = -1 ;
  } else {
    state->firstband = state->lastband ;
  }

  return TRUE ;
}

/* We need to add these to an otherwise "full" buffer */
# define HWA1_RESERVE (sizeof(gipa2_com_block_end) + sizeof(gipa2_band_dma))

/* Check if there's enough room in the buffer for the command.
 * If not, send the buffer and get a new one, and return TRUE.
 * Otherwise it will fit, so we return FALSE.
 */
static Bool wont_fit(hwa1_state_t *state, size_t length)
{

  if (state->ptr + length <= state->end - HWA1_RESERVE)
    return FALSE ;  /* No, that'll fit */

  /* Buffer is full, submit it and initialise a new one */
  if (!hwa1_submit_buffer(state, FALSE))
    return TRUE ;  /* Failure signalled already */

  if (!hwa1_new_buffer(state))
    return TRUE ;  /* Failure signalled already */

  return TRUE ;  /* Yes, didn't fit (but now will) */
}
/* -------------------------------------------------------------------------- */
/* Some ROP stuff */

/* An illegal ROP2 value */
#define ROP2_INVALID 1

/* Table of ROP2 equivalent of ROP3s, or ROP2_INVALID */
static uint8 ROP2[256] ;

/* Initialise the tables. They could be statically defined but this is less
   prone to typos.
 */
static void init_ROP2(void)
{
  int i ;
  for (i = 0 ; i < 256 ; ++i) {
    ROP2[i] = ROP2_INVALID ;
  }
  /* This is not a 1:1 mapping as HWA1 has no T, only S */
  ROP2[ROP3_Blk]  = ROP2_Blk ;
  ROP2[ROP3_DSan] = ROP2_DSan ;
  ROP2[ROP3_DTan] = ROP2_DTan ;
  ROP2[ROP3_DSno] = ROP2_DSno ;
  ROP2[ROP3_DTno] = ROP2_DTno ;
  ROP2[ROP3_Sn]   = ROP2_Sn ;
  ROP2[ROP3_Tn]   = ROP2_Tn ;
  ROP2[ROP3_SDno] = ROP2_SDno ;
  ROP2[ROP3_TDno] = ROP2_TDno ;
  ROP2[ROP3_Dn]   = ROP2_Dn ;
  ROP2[ROP3_DSxn] = ROP2_DSxn ;
  ROP2[ROP3_TDxn] = ROP2_TDxn ;
  ROP2[ROP3_DSon] = ROP2_DSon ;
  ROP2[ROP3_DTon] = ROP2_DTon ;
  ROP2[ROP3_DSo]  = ROP2_DSo ;
  ROP2[ROP3_DTo]  = ROP2_DTo ;
  ROP2[ROP3_DSx]  = ROP2_DSx ;
  ROP2[ROP3_DTx]  = ROP2_DTx ;
  ROP2[ROP3_D]    = ROP2_D ;
  ROP2[ROP3_DSna] = ROP2_DSna ;
  ROP2[ROP3_DTna] = ROP2_DTna ;
  ROP2[ROP3_S]    = ROP2_S ;
  ROP2[ROP3_T]    = ROP2_T ;
  ROP2[ROP3_SDna] = ROP2_SDna ;
  ROP2[ROP3_TDna] = ROP2_TDna ;
  ROP2[ROP3_DSa]  = ROP2_DSa ;
  ROP2[ROP3_DTa]  = ROP2_DTa ;
  ROP2[ROP3_Wht]  = ROP2_Wht ;
}

/* RDR Resources */

static Bool hwa1_resources(hwa1_state_t * state)
{
  void * vp ;
  size_t len ;
  int bitdepth = state->config.bitDepth ;

  /* TODO: Now this is called WITHIN the lifetime of the hwa1_state, we can
   * change all these globals into state members.
   */

  /* Find the various color management tables we should use.
   *
   * We carry on regardless if these aren't supplied, but we'll
   * omit commands or use black as appropriate. */
  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_CMM, bitdepth, &vp, 0) ;
  hwa_cmm_lut = vp ;

  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_BLACK, bitdepth, &vp, 0) ;
  hwa_black_lut = vp ;

  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_GAMMA, bitdepth, &vp, 0) ;
  hwa_gamma_table = vp ;

  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_USER, bitdepth, &vp, 0) ;
  hwa_user_table = vp ;

  /* TODO: We may need multiple TYPES of stretchblt dither - for images,
   * chars etc - so the RDR ID may have to be calculated from the bitdepth
   * and a type number, so we can find all different types, or a new RDR Type
   * such as RDR_HWA_GIPA2_TEXTDITHER should be defined. */
  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_DITHER, bitdepth, &vp, 0);
  hwa_dither = vp ;

  vp = NULL ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_COLORS, bitdepth, &vp, 0);
  hwa_colors = vp ;

  len = 1023 ;
  (void)SwFindRDR(RDR_CLASS_HWA, RDR_HWA_GIPA2_TONERLIMIT, bitdepth,
    0, &len) ;
  if (len < 255 || len > 1023)
    len = 1023 ;
  hwa_toner_limit = (uint16)len ;

  return TRUE ;
}

/* Lifecycle management */

static Bool hwa1_select(surface_instance_t **instance, const sw_datum *pagedict,
  const sw_data_api *dataapi, Bool continued)
{
  UNUSED_PARAM(surface_instance_t **, instance) ;
  UNUSED_PARAM(const sw_datum *, pagedict) ;
  UNUSED_PARAM(const sw_data_api *, dataapi) ;

  if (!continued) {
    void * vp ;
    
    /* Find the HWA API. If there isn't one we can't continue. */
    if (SwFindRDR(RDR_CLASS_API, RDR_API_HWA, 20140721, &vp, NULL)
      != SW_RDR_SUCCESS || vp == 0)
      return error_handler(UNREGISTERED) ;   /* HWA API is required */
    hwa_api = (hwa_api_20140721 *)vp ;

#   ifdef HWA1_METRICS
    hwa1_metrics_reset(0) ;
#   endif
  }

  return TRUE ;
}

/* ========================================================================== */
/** State initialisation and finalisation. */

static void hwa1_state_reset(hwa1_state_t * state)
{
  /* Reset the graphic state */

  state->DL_ROP2 = ROP2_INVALID ;
  state->HWA_ROP2 = ROP2_INVALID ;
  
  state->DL_color = PCL_PACKED_RGB_BLACK ;   /* must not == DL_color! */
  state->HWA_color = PCL_PACKED_RGB_INVALID ; 

  bbox_clear(&state->rectclip) ;
  bbox_clear(&state->bandclip) ;
  bbox_clear(&state->DL_clipbox) ;

  state->DL_clipid = SURFACE_CLIP_INVALID ;
  state->HWA_clipid = SURFACE_CLIP_INVALID ;
}


static Bool hwa1_state_init(hwa1_state_t *state)
{
  static hwa_options default_config = {
    HWA_NO_ADDRESS, /* No VCOMAD */
    HWA_NO_ADDRESS, /* No VDATAAD */
    1,              /* Default bitdepth */
    0               /* No options */
  } ;

  HqMemZero(state, sizeof(*state)) ;
  /* Get the configuration options. */
  state->config = default_config ;
  if (hwa_api->get_options) {
    if (hwa_api->get_options(state->timeline, &state->config) != HWA_SUCCESS)
      return FALSE ;
  }
  /* Disallow mutually incompatible options */
  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) != 0)
    state->config.options &= ~HWA1_OPT_DEFER_BUFFER_ASSETS ;
  /* Check addresses */
  if (state->config.VCOMAD == HWA_AUTO_ADDRESS) {
    HQFAIL("Can't set VCOMAD to AUTO_ADDRESS") ;
    state->config.VCOMAD = HWA_NO_ADDRESS ;
  }
  if (state->config.VDATAAD == HWA_AUTO_ADDRESS &&
      state->config.VCOMAD == HWA_NO_ADDRESS) {
    HQFAIL("Can't set VDATAAD to AUTO_ADDRESS if VCOMAD is NO_ADDRESS") ;
    state->config.VDATAAD = HWA_NO_ADDRESS ;
  }
  /* Check bit depth */
  switch (state->config.bitDepth) {
  case 1:
  case 2:
  case 4:
    break ;
  default:
    HQFAIL("Invalid HWA bit depth selected. Using 1bpp.") ;
    state->config.bitDepth = 1 ;
    break ;
  }

  /* Get or update the resources - dithers, gamma, LUTs etc */
  if (!hwa1_resources(state))
    return FALSE ;

  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) == 0) {
    /* Not all one band, so allocate and initialise buffer now. */
    if ( !hwa1_new_buffer(state))
      return FALSE ;
  }

  hwa1_state_reset(state) ;
  state->error = HWA1_OK ;

  NAME_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  return TRUE ;
}

static Bool hwa1_state_finish(hwa1_state_t *state)
{
  if (state->error != HWA1_OK) {
    /* Something went badly wrong */
    UNNAME_OBJECT(state) ;
    return FALSE ;
  }

  /* Push out the final buffer. */
  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) != 0)
    hwa1_band_end(state, TRUE) ;

  hwa1_submit_buffer(state, TRUE) ;

  UNNAME_OBJECT(state) ;

  return TRUE ;
}

/* ========================================================================== */
/** Band start and end

    GIPA2 only allows one BAND_INIT/BAND_DMA per Order List, so each "Band"
    must consist of:

      COM_HEAD
      BAND_INIT
      ...
      BAND_DMA
      COM_BLOCK_END

    So we check there's enough room in the buffer for all of that and some
    for actual commands.

    We may decide to always insert SET_GAMMA_STRETCHBLT etc at this point too,
    or perhaps we put them in at the start to cover all bands?
*/

#define HWA_MINIMUM (sizeof(gipa2_com_head) + sizeof(gipa2_band_init) + \
                     sizeof(gipa2_band_dma) + sizeof(gipa2_com_block_end))

static Bool hwa1_band_start(hwa1_state_t * state, const render_state_t *rs)
{
  int32 band_id = 0 ;
  int bitdepth = state->config.bitDepth ;
  enum {
    NOT_TRIED, SUBMIT, SUBMITTED
  } submit = NOT_TRIED ;

  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) == 0)
    band_id = rs->forms->retainedform.hoff / rs->page->band_lines ;

  state->bandid = band_id ;            /* And not rs->band as I was expecting */
  state->bandwidth = rs->page->page_w ;
  state->bandheight = rs->page->band_lines ;
  state->pageheight = rs->page->page_h ;

  /* Lazy buffer allocation */
  if (state->ptr == NULL && !hwa1_new_buffer(state)) {
    state->error = HWA1_ERROR_BUFFER ;
    return FALSE ;
  }

  /* Check there's enough room in the buffer for a BAND_INIT/BAND_DMA and some
   * commands, otherwise we may as well send off what we have so far */
  if (wont_fit(state, HWA_MINIMUM + 1024)) {
    /* We now have a new buffer or a fatal error */
    if (state->error != HWA1_OK)
      return FALSE ;
  }

  if (state->firstband == -1) {
    state->firstband = state->bandid ;
  } else {
    /* HWA API submit buffer call requires contiguous bands, so if this isn't,
     * submit what we have already. */
    if (state->bandid != state->lastband + state->complete) {
      if (!hwa1_submit_buffer(state, TRUE))
        return FALSE ;
      if (!hwa1_new_buffer(state)) {
        state->error = HWA1_ERROR_BUFFER ;
        return FALSE ;
      }
      state->firstband = state->bandid ;
    }
  }
  state->lastband = state->bandid ;

  /* If all-one-band, we're done. */
  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) != 0)
    return TRUE ;

  do {
    gipa2_band_init * bandinit ;
    uint8 * C, * M, * Y, * K ;
    gipa2_com_head * head ;
    hwa_result result ;
    size_t align ;
    int32 rows ;

    /* This may fail if there are no more buffers, in which case we'll need to
     * submit the buffer as-is and try again */
    result = hwa_api->get_bands(state->timeline, state->buffer, band_id,
                                &C, &M, &Y, &K) ;
    switch (result) {
    case HWA_SUCCESS:
      /* First the new COM_HEAD, pointed to by the previous if there was one.
         Note that these must be 16 byte aligned, apparently. */
      align = state->ptr - state->buffer ;
      align = (align + 15) & ~15 ;
      state->ptr = state->buffer + align ;
;
      GET_CMD(head, state, GIPA2_COM_HEAD) ;
      NOP(head->nop1) ;
      NOP(head->nop2) ;
      NOP(head->nop3) ;
      head->next = 0 ;
      if (state->head)
        state->head->next = (gipa2_com_head *)PTR32(head) ;
      state->head = head ;
      NOP(head->nop4) ;
      NOP(head->nop5) ;

      /* Now the BAND_INIT with the band addresses we've been given */
      GET_CMD(bandinit, state, GIPA2_BAND_INIT) ;
      NOP(bandinit->nop1) ;
      NOP(bandinit->nop2) ;
      NOP(bandinit->nop3) ;
      /* It was a lot easier when the Skin had to initialise these... */
      bandinit->planes = GIPA2_CMYK ;
      bandinit->bitdepth = (bitdepth == 1) ? GIPA2_1BIT :
                           (bitdepth == 2) ? GIPA2_2BIT : GIPA2_4BIT ;

#     ifdef HWA1_TRUNCATE_FINAL_BAND
      /* Final band may not be full height */
      rows = state->pageheight - (state->bandid * state->bandheight) ;
      if (rows < 1)
        rows = 1 ;
      if (rows > state->bandheight)
#     endif
        rows = state->bandheight ;

      bandinit->pixels = HWA32(state->bandwidth) ;
      bandinit->bytes  = HWA32(((state->bandwidth * bitdepth + 63) / 64) * 8) ;
      bandinit->rows   = HWA32(rows) ;
      bandinit->offset = HWA32(state->bandid * state->bandheight) ;

      /* Page width and page height aren't used, but may be useful for debug */
      bandinit->nop2   = HWA32(state->bandwidth) ;
      bandinit->nop3   = HWA32(state->pageheight) ;

      /* The band addresses from the Skin */
      bandinit->kband = PTR32(K) ;
      bandinit->cband = PTR32(C) ;
      bandinit->mband = PTR32(M) ;
      bandinit->yband = PTR32(Y) ;

      /* We have started a(nother) band, so this final band is incomplete */
      state->complete = FALSE ;

#     ifdef HWA1_BAND_RESETS_STATE
        hwa1_state_reset(state) ;
#     endif

      if ((state->config.options & HWA1_OPT_DEFER_BUFFER_ASSETS) != 0) {
        /* Definition of buffer assets was defered until now. */
        if (!hwa1_buffer_assets(state, FALSE))
          return FALSE ;
      }

      return TRUE ;

    case HWA_ERROR_MEMORY: /* No band buffers left - submit buffer, try again */
      if (submit >= SUBMIT){
        HQFAIL("Out of band memory at start of band") ;
        return FALSE ;
      }
      submit = SUBMIT ;
      break ; /* loops round again */

    case HWA_ERROR_UNKNOWN:
      HQFAIL("HWA API refused band parameters") ;
      return FALSE ;
    case HWA_ERROR_SYNTAX:
      HQFAIL("HWA API syntax error") ;
      return FALSE ;
    case HWA_ERROR_IN_USE:
      HQFAIL("HWA API says band already complete") ;
      return FALSE ;
    case HWA_ERROR:
      HQFAIL("HWA API hardware failure") ;
      return FALSE ;
    default:
      HQFAIL("Unanticipated HWA API error") ;
      return FALSE ;
    }

    if (submit == SUBMIT) {
      Bool ok = TRUE ;
      /* If we have to submit the buffer it *doesn't* have lastband in it yet,
       * so temporarily decrement it. */
      --state->lastband ;
      /* No point submitting an empty buffer, though this shouldn't happen. */
      if (state->lastband >= state->firstband)
        ok = hwa1_submit_buffer(state, TRUE) ;
      /* Update the new band range to just this band. */
      state->firstband = state->lastband = band_id ;
      if (!ok)
        return FALSE ;
      if (!hwa1_new_buffer(state)) {
        state->error = HWA1_ERROR_BUFFER ;
        return FALSE ;
      }
      submit = SUBMITTED ;
    }
  } while (TRUE) ;

  /* never reached */
}

/* ========================================================================== */
/** Band localiser - brackets the rendering of a band */

static Bool hwa1_band_localise(surface_handle_t *handle,
                               const render_state_t *rs,
                               render_band_callback_fn *callback,
                               render_band_callback_t *data,
                               surface_bandpass_t *bandpass)
{
# ifdef HWA1_METRICS
  size_t band_buffers, band_cmds, band_assets, band_bytes ;
# endif
  Bool result ;
  hwa1_state_t * state = &handle->sheet->state ;

  UNUSED_PARAM(const render_state_t *, rs) ;
  UNUSED_PARAM(surface_bandpass_t *, bandpass) ;

  if (!hwa1_band_start(state, rs))
    return FALSE ;

# ifdef HWA1_METRICS
  band_buffers = hwa1_metrics.this_page_buffers ;
  band_cmds = hwa1_metrics.this_page_cmds ;
  band_assets = hwa1_metrics.this_page_assets ;
  band_bytes = hwa1_metrics.this_page_bytes ;
# endif

  /* Actually do the band render callback */
  result = (*callback)(data) ;

  if (state->error != HWA1_OK) {
    /* Something went badly wrong */
    return FALSE ;
  }

  if ((state->config.options & HWA1_OPT_ALL_ONE_BAND) != 0) {
    /* Not sure this is necessary */
    gipa2_reset_clip * reset ;
    GET_CMD(reset, state, GIPA2_RESET_CLIP) ;
    NOP(reset->nop1) ;
    NOP(reset->nop2) ;
    NOP(reset->nop3) ;
  } else {
    if (!hwa1_band_end(state, TRUE))
      return FALSE ;
  }

  if (state->error != HWA1_OK) {
    /* Something went badly wrong */
    return FALSE ;
  }

# ifdef HWA1_METRICS
  band_buffers = hwa1_metrics.this_page_buffers - band_buffers ;
  band_cmds = hwa1_metrics.this_page_cmds - band_cmds ;
  band_assets = hwa1_metrics.this_page_assets - band_assets ;
  band_bytes = hwa1_metrics.this_page_bytes - band_bytes ;

  if ( band_buffers > hwa1_metrics.peak_band_buffers )
    hwa1_metrics.peak_band_buffers = band_buffers ;
  if ( band_cmds > hwa1_metrics.peak_band_cmds )
    hwa1_metrics.peak_band_cmds = band_cmds ;
  if ( band_assets > hwa1_metrics.peak_band_assets )
    hwa1_metrics.peak_band_assets = band_assets ;
  if ( band_bytes > hwa1_metrics.peak_band_bytes )
    hwa1_metrics.peak_band_bytes = band_bytes ;
# endif

  return result ;
}

/* ========================================================================== */
/** Sheet begin - used to reset page rendering metrics. */

static Bool hwa1_sheet_begin(surface_handle_t *handle, sheet_data_t *sheet_data)
{
  surface_sheet_t * hwasheet ;
  hwa1_state_t * state ;

  UNUSED_PARAM(sheet_data_t *, sheet_data) ;

# ifdef HWA1_METRICS
  hwa1_metrics.this_page_buffers = 0 ;
  hwa1_metrics.this_page_cmds = 0 ;
  hwa1_metrics.this_page_assets = 0 ;
  hwa1_metrics.this_page_bytes = 0 ;

  if (TRUE) { /* PROTOTEST(hwa1_metrics) > 2 */
    int i ;
    for ( i = 0 ; i < N_GIPA2_OPCODES ; ++i ) {
      hwa1_metrics.opcodes[i].count = 0 ;
      hwa1_metrics.opcodes[i].cmds = 0 ;
      hwa1_metrics.opcodes[i].assets = 0 ;
    }
  }
# endif

  hwasheet = (surface_sheet_t *) mm_alloc(mm_pool_temp, sizeof(*hwasheet),
                                          MM_ALLOC_CLASS_HWA1) ;
  if ( hwasheet == NULL )
    return error_handler(VMERROR) ;
  state = &hwasheet->state ;

  if ( !hwa1_state_init(state) ) {
    mm_free(mm_pool_temp, hwasheet, sizeof(*hwasheet)) ;
    return FALSE ;
  }
  state->firstband = state->lastband = -1 ;

  handle->sheet = hwasheet ;

  return TRUE ;
}

#ifdef HWA1_METRICS
static const char *hwa1_opcode_name(int i)
{
  static char buffer[100] ;

  switch ( i ) {
  case GIPA2_COM_HEAD:              return "GIPA2_COM_HEAD" ;
  case GIPA2_PAGE_INIT:             return "GIPA2_PAGE_INIT" ;
  case GIPA2_BAND_INIT:             return "GIPA2_BAND_INIT" ;
  case GIPA2_DRAW_RUN:              return "GIPA2_DRAW_RUN" ;
  case GIPA2_DRAW_STRETCHBLT:       return "GIPA2_DRAW_STRETCHBLT" ;
  case GIPA2_DRAW_BITBLT:           return "GIPA2_DRAW_BITBLT" ;
  case GIPA2_DRAW_RUN_REPEAT:       return "GIPA2_DRAW_RUN_REPEAT" ;
  case GIPA2_SET_ROP2:              return "GIPA2_SET_ROP2" ;
  case GIPA2_RESET_CLIP:            return "GIPA2_RESET_CLIP" ;
  case GIPA2_SET_COLOR:             return "GIPA2_SET_COLOR" ;
  case GIPA2_SET_DITHER_STRETCHBLT: return "GIPA2_SET_DITHER_STRETCHBLT" ;
  case GIPA2_SET_GAMMA_STRETCHBLT:  return "GIPA2_SET_GAMMA_STRETCHBLT" ;
  case GIPA2_SET_DITHER_SIZE:       return "GIPA2_SET_DITHER_SIZE" ;
  case GIPA2_SET_CLIP:              return "GIPA2_SET_CLIP" ;
  case GIPA2_SET_INDEXCOLOR:        return "GIPA2_SET_INDEXCOLOR" ;
  case GIPA2_SET_PENETRATIONCOLOR:  return "GIPA2_SET_PENETRATIONCOLOR" ;
  case GIPA2_SET_BRUSH:             return "GIPA2_SET_BRUSH" ;
  case GIPA2_BAND_DMA:              return "GIPA2_BAND_DMA" ;
  case GIPA2_COM_BLOCK_END:         return "GIPA2_COM_BLOCK_END" ;
  }

  swcopyf((uint8 *)buffer, (uint8 *)"GIPA2_OPCODE_%d", i) ;
  return buffer ;
}
#endif

/** Sheet end - used to gather page rendering metrics. */

static Bool hwa1_sheet_end(surface_handle_t handle, sheet_data_t *sheet_data,
                           Bool result)
{
  surface_sheet_t *hwasheet = handle.sheet ;

  UNUSED_PARAM(sheet_data_t *, sheet_data) ;

  if ( !hwa1_state_finish(&hwasheet->state) )
    result = FALSE ;
  mm_free(mm_pool_temp, hwasheet, sizeof(*hwasheet)) ;

# ifdef HWA1_METRICS
  if ( hwa1_metrics.this_page_buffers > 0 ) {
    monitorf((uint8 *)("HWA1 page buffers %u, bytes %u (commands %u, assets %u)\n"),
             (uint32)hwa1_metrics.this_page_buffers,
             (uint32)hwa1_metrics.this_page_bytes,
             (uint32)hwa1_metrics.this_page_cmds,
             (uint32)hwa1_metrics.this_page_assets) ;

    if ( TRUE /*PROTOTEST(hwa1_metrics) > 2*/ ) {
      int i ;
      for ( i = 0 ; i < N_GIPA2_OPCODES ; ++i ) {
        if ( hwa1_metrics.opcodes[i].count > 0 ) {
          monitorf((uint8 *)("HWA1 opcode %s count %u (commands %u, assets %u)\n"),
                   hwa1_opcode_name(i),
                   (uint32)hwa1_metrics.opcodes[i].count,
                   (uint32)hwa1_metrics.opcodes[i].cmds,
                   (uint32)hwa1_metrics.opcodes[i].assets) ;
        }
      }
    }
  }

  if ( hwa1_metrics.this_page_buffers > hwa1_metrics.peak_page_buffers )
    hwa1_metrics.peak_page_buffers = hwa1_metrics.this_page_buffers ;
  if ( hwa1_metrics.this_page_cmds > hwa1_metrics.peak_page_cmds )
    hwa1_metrics.peak_page_cmds = hwa1_metrics.this_page_cmds ;
  if ( hwa1_metrics.this_page_assets > hwa1_metrics.peak_page_assets )
    hwa1_metrics.peak_page_assets = hwa1_metrics.this_page_assets ;
  if ( hwa1_metrics.this_page_bytes > hwa1_metrics.peak_page_bytes )
    hwa1_metrics.peak_page_bytes = hwa1_metrics.this_page_bytes ;

  hwa1_metrics.total_buffers += hwa1_metrics.this_page_buffers ;
  hwa1_metrics.total_cmds += hwa1_metrics.this_page_cmds ;
  hwa1_metrics.total_assets += hwa1_metrics.this_page_assets ;
  hwa1_metrics.total_bytes += hwa1_metrics.this_page_bytes ;
# endif

  return result ;
}

/* ========================================================================== */
/* Color conversion. HWA1 only supports CMYK colors for span and block, but
   RGB for images. We perform a naive RGB to CMYK when setting up the span
   color. */
static inline void rgbToCmyk(uint8 cmyk[4], PclPackedColor rgb)
{
  uint8 c = 255 - (uint8)rgb ;
  uint8 m = 255 - (uint8)(rgb >> 8) ;
  uint8 y = 255 - (uint8)(rgb >> 16) ;
  uint8 k = min(c, m) ; k = min(k, y) ;
  /* GIPA2 order is CMYK */
  cmyk[0] = c-k ; cmyk[1] = m-k ; cmyk[2] = y-k ; cmyk[3] = k ;
}

/* For K only mode - gray actually goes in the C byte */

static inline void rgbToGray(uint8 cmyk[4], PclPackedColor rgb)
{
  uint8 r = (uint8)rgb ;
  uint8 g = (uint8)(rgb >> 8) ;
  uint8 b = (uint8)(rgb >> 16) ;
  /* RGB weights are 0.213, 0.715, 0.072, scaled appropriately */
  uint32 k = (r * 14014) + (g * 47042) + (b * 4737) ;
  /* Only first is used for gray */
  cmyk[0] = 255 & (k >> 16) ; cmyk[1] = 0 ; cmyk[2] = 0 ; cmyk[3] = 0 ;
}

/* ========================================================================== */

/** Check for objects which do not use foreground, and see if their ROPs
    are reducible to cases we can handle. */
static Bool hwa1_rop_support(surface_page_t *handle,
                             const DL_STATE *page,
                             const dl_color_t *color,
                             PclAttrib *attrib)
{
  UNUSED_PARAM(surface_page_t *, handle) ;
  UNUSED_PARAM(const DL_STATE *, page) ;
  UNUSED_PARAM(const dl_color_t *, color) ;

  /* HWA1 only supports ROP2, which works on D and S. We can convert T to S
     if S is not used, so we only backdrop render if both T and S are both
     required. */
  attrib->backdrop = (pclROPRequiresSource(attrib->rop) &&
                      pclROPRequiresTexture(attrib->rop)) ;
  HQASSERT(attrib->backdrop || ROP2[attrib->rop] != ROP2_INVALID,
           "Can't handle ROP2 replacement for ROP3") ;

  /* We can handle small black and white patterns using the GIPA brush. Other
     patterns require ROP T and the pattern blitters. */
  switch ( attrib->patternColors ) {
  case PCL_PATTERN_BLACK_AND_WHITE:
    if ( attrib->constructionState.targetSize.x <= 64 &&
         attrib->constructionState.targetSize.y <= 64 ) {
      attrib->patternBlit = FALSE ;
      break ;
    }
    /*@fallthrough@*/
  default:
    if ( attrib->rop == PCL_ROP_T ) {
      attrib->patternBlit = TRUE ;
    } else { /* Can't handle in pattern blitter. */
      attrib->backdrop = TRUE ;
    }
    break ;
  case PCL_PATTERN_NONE:
    attrib->patternBlit = FALSE ;
    break ;
  }

  return TRUE ;
}

/* ========================================================================== */
/* ROP command */

/* If we need to update the ROP, return the number of bytes required */
static size_t ROP2_required(hwa1_state_t *state)
{
  if (state->HWA_ROP2 != state->DL_ROP2)
    return sizeof(gipa2_set_rop2) ;

  return 0 ;
}

/* Add a ROP command if we need to */
static void ROP2_update(hwa1_state_t *state)
{
  if (state->HWA_ROP2 != state->DL_ROP2) {
    gipa2_set_rop2 *rop ;
    GET_CMD(rop, state, GIPA2_SET_ROP2) ;
    rop->rop2   = (uint8) state->DL_ROP2 ;
    NOP(rop->nop1) ;
    NOP(rop->nop2) ;

    state->HWA_ROP2 = rop->rop2 ;
  }
}

/* ========================================================================== */
/* Source transparency (penetration color) command */

/* If we need to update the ROP, return the number of bytes required */
static size_t pcolor_required(hwa1_state_t *state)
{
  if (state->sTrans)
    return sizeof(gipa2_set_penetration_color) ;

  return 0 ;
}

/* Add a ROP command if we need to */
static void pcolor_update(hwa1_state_t *state)
{
  if (state->sTrans) {
    gipa2_set_penetration_color *pcolor ;
    GET_CMD(pcolor, state, GIPA2_SET_PENETRATIONCOLOR) ;
    NOP(pcolor->nop1) ;
    NOP(pcolor->nop2) ;
    NOP(pcolor->nop3) ;
    /* NET_BSD doesn't like type punning, so can't use PCL_PACKED_CMYK_WHITE
     * directly... */
    pcolor->c[0] = 255 & PCL_PACKED_CMYK_WHITE ;
    pcolor->c[1] = 255 & PCL_PACKED_CMYK_WHITE ;
    pcolor->c[2] = 255 & PCL_PACKED_CMYK_WHITE ;
    pcolor->c[3] = 255 & PCL_PACKED_CMYK_WHITE ;
  }
}

/* -------------------------------------------------------------------------- */
/* Brush command */

/* If we need to update the brush, return the number of bytes required */
static size_t brush_required(hwa1_state_t *state)
{
  size_t required = 0 ;

  if (state->HWA_pattern != state->DL_pattern ||
      state->HWA_pTrans != state->DL_pTrans) {
    /** \todo ajcd 2013-08-12: Assume that we need GIPA2_SET_BRUSH to clear
        the brush, but we can have one with no data and zero width and
        height. */
    if ( state->DL_pattern != NULL ) {
      uint8 *data = hwa1_find_asset(state, state->DL_pattern, 0) ;
      if ( data == NULL )
        required += sizeof(assetlist) + 64 * 8 ; /* 64x64bit */
    }

    required += sizeof(gipa2_set_brush) ;
  }

  return required ;
}

/* Add a brush command if we need to */
static void brush_update(DL_STATE *page, hwa1_state_t *state)
{
  if (state->HWA_pattern != state->DL_pattern ||
      state->HWA_pTrans != state->DL_pTrans) {
    gipa2_set_brush *brush ;

    GET_CMD(brush, state, GIPA2_SET_BRUSH) ;
    NOP(brush->nop1) ;
    NOP(brush->nop2) ;
    NOP(brush->nop3) ;
    brush->flags   = state->DL_pTrans ? GIPA2_BRUSH_TRANS : GIPA2_BRUSH_OPAQUE ;
    brush->xoffset = 0 ;
    brush->yoffset = 0 ;

    if ( state->DL_pattern != NULL ) {
      PclDLPattern* pattern = state->DL_pattern;
      uint8 *data = hwa1_find_asset(state, pattern, 0) ;
      if (!data) {
        uint32 x, y ;
        dl_color_t dlc_white ;
        p_ncolor_t nc_white ;
        PclDLPatternIterator iterator ;

        HQASSERT(pattern->preconverted == PCL_PATTERN_PRECONVERT_DEVICE,
                 "PCL DL pattern is not preconverted") ;

        dlc_clear(&dlc_white) ;
        dlc_get_white(page->dlc_context, &dlc_white) ;
        dlc_to_dl_weak(&nc_white, &dlc_white) ;

        data = hwa1_add_asset(state, ITSA_PATTERN, pattern, 64*8, 0) ;
        HQASSERT(data, "Couldn't allocate brush asset") ;
#       ifdef HWA1_METRICS
          hwa1_metrics.opcodes[GIPA2_SET_BRUSH].assets += 64 * 8 ;
#       endif

        for ( y = 0 ; y < 64 ; ++y ) {
          dcoord w = 64 ;
          pclDLPatternIteratorStart(&iterator, pattern, 0, y, w) ;
          for ( x = 0 ; x < 64 ; ) {
            int32 count ;
            for ( count = iterator.cspan ; count > 0 ; --count, ++x ) {
              uint8 mask = (1 << (x & 7)) ;
              if ( iterator.color.ncolor == nc_white )
                data[y * 8 + (x >> 3)] &= ~mask ;
              else
                data[y * 8 + (x >> 3)] |= mask ;
            }

            w -= iterator.cspan ;
            if ( w <= 0 )
              break ;

            pclDLPatternIteratorNext(&iterator, w) ;
          }
        }
      }
      brush->width   = CAST_UNSIGNED_TO_UINT8(pattern->width) ;
      brush->height  = CAST_UNSIGNED_TO_UINT8(pattern->height) ;
      brush->data    = PTR32(data) ;
    } else {
      brush->width   = 0 ;
      brush->height  = 0 ;
      brush->data    = NULL ;
    }

    state->HWA_pattern = state->DL_pattern ;
  }
}


/* Generate a dither cell from a threshold matrix.
 *
 * options: b0 reflect X axis
 *          b1 reflect Y axis
 *          b2 GIPA2 X ordering (x0,x1,0,0,x2,x3,0,0... 128 bytes per line)
 */

uint8 * hwa1_cell(hwa1_state_t * state, uint8 * threshold, int w, int h,
                  uint8 * type, int level, int options)
{
  uint8 * cell ;
  int i, j, instride, outstride ;
  int bitdepth = state->config.bitDepth ;
  int levels = 1 << bitdepth ;

  cell = hwa1_add_asset(state, ITSA_CELL, type, HWA1_CELL_SIZE(w, h), level) ;
  if (!cell)
    return NULL ;

  /* Adjustment for thresholding with >1 bit depth */
  level = (level * (257 - levels) + 127) >> 8 ;

  instride = (options & 4) ? 128 : (w + 3) &~3 ;
  outstride = ((w * bitdepth + 31) &~31) >> 3 ;

  for (j = 0 ; j < h ; ++j) {
    int y = (options & 2) ? h - j - 1 : j ;
    int bits = 0 ;
    uint32 shift = 0 ;
    uint8 * in = threshold + y * instride ;
    uint8 * pad, * out = pad = cell + y * outstride ;

    for (i = 0 ; i < w ; ++i) {
      int x = (options & 1) ? w - i - 1 : i ;
      int t = (options & 4) ? in[x * 2 - (x & 1)] : in[x] ;
      t = level - t ;
      if (t < 0)
        t = 0 ;
      if (t >= levels)
        t = levels - 1 ;
      shift = (shift << bitdepth) | t ;
      bits += bitdepth ;
      if (bits >= 8) {
        bits -= 8 ;
        *out++ = 255 & (shift >> bits) ;
        shift &= (1 << bits) - 1 ;
      }
    }
    /* Final word of a line needs to be padded with bits from first word */
    if (bits > 0 || ((out-pad)&3) != 0) {
      in = pad ;
      do {
        *out++ = 255 & ((shift << (8 - bits)) | (*in >> bits)) ;
        shift = *in++ ;
      } while (((out - pad) & 3) != 0) ;
    }
  }

  return cell ;
}

/* -------------------------------------------------------------------------- */
/* Color command */

/* If we need to update the color, return the number of bytes required */
static size_t color_required(hwa1_state_t * state, Bool indexed)
{
  size_t required ;

  if (state->HWA_color == state->DL_color)
    return 0 ;

  if (indexed)
    return sizeof(gipa2_set_indexcolor) ;

  /* Size required for the SET_COLOR command depends on the strategy used for
    * defining dither cells - if they're already in the HWA's memory or
    * or we've predefined the whole lot as a buffer asset, we only need the
    * command. Otherwise we have to look for the appropriate cell assets and
    * calculate how much space we need to add missing ones... except that if
    * the absence of say a yellow cell results in us starting a new buffer,
    * we necessarily have to redefine the other cells anyway. Anyway, this is
    * used to work out if it'll fit, not how much will actually be taken up.
    */
  required = sizeof(gipa2_set_color) ;

  if (!state->Kdither) {
    /* No predefined/preloaded cells, so we're into asset management. */
    uint8 * C, * M, * Y, * K ;
    uint8 cmyk[4] ;
    rgbToCmyk(cmyk, state->DL_color) ;

    C = hwa1_find_asset(state, CELL_C, cmyk[0]) ;
    M = hwa1_find_asset(state, CELL_M, cmyk[1]) ;
    Y = hwa1_find_asset(state, CELL_Y, cmyk[2]) ;
    K = hwa1_find_asset(state, CELL_K, cmyk[3]) ;

    /* How big the missing cells will be depends on whether we have
     * the sizes defined.
     */
    if (hwa_colors) {
      /* The dither cells are defined but are to be included on demand */
      if (!C) required += HWA1_CELL_SIZE(hwa_colors->Cwidth,
                                         hwa_colors->Cheight) ;
      if (!M) required += HWA1_CELL_SIZE(hwa_colors->Mwidth,
                                         hwa_colors->Mheight) ;
      if (!Y) required += HWA1_CELL_SIZE(hwa_colors->Ywidth,
                                         hwa_colors->Yheight) ;
      if (!K) required += HWA1_CELL_SIZE(hwa_colors->Kwidth,
                                         hwa_colors->Kheight) ;

#ifdef HWA1_USE_THRESHOLDS
    } else if (hwa_dither) {
      /* Create dither cells from the defined STRETCHBLT threshold data. */
      if (!C) required += HWA1_CELL_SIZE(hwa_dither->Cwidth,
                                         hwa_dither->Cheight) ;
      if (!M) required += HWA1_CELL_SIZE(hwa_dither->Mwidth,
                                         hwa_dither->Mheight) ;
      if (!Y) required += HWA1_CELL_SIZE(hwa_dither->Ywidth,
                                         hwa_dither->Yheight) ;
      if (!K) required += HWA1_CELL_SIZE(hwa_dither->Kwidth,
                                         hwa_dither->Kheight) ;
#endif

    } else {
      /* Fake minimal cells */
      if (!C) required += HWA1_CELL_SIZE(16, 16) ;
      if (!M) required += HWA1_CELL_SIZE(16, 16) ;
      if (!Y) required += HWA1_CELL_SIZE(16, 16) ;
      if (!K) required += HWA1_CELL_SIZE(16, 16) ;
    }
  }

  return required ;
}

/* Add a color command if we need to */
static void color_update(hwa1_state_t * state, Bool indexed)
{
  gipa2_set_color * color ;
  uint8 cmyk[4] ;

  if (state->HWA_color == state->DL_color)
    return ;
  state->HWA_color = state->DL_color ;

  if (indexed) {
    gipa2_set_indexcolor * index ;
    GET_CMD(index, state, GIPA2_SET_INDEXCOLOR) ;
    NOP(index->nop1) ;
    NOP(index->nop2) ;
    NOP(index->nop3) ;
    index->c0[0] = 0 ;
    index->c0[1] = 0 ;
    index->c0[2] = 0 ;
    index->c0[3] = 0 ;
    rgbToCmyk(index->c1, state->DL_color) ;

    return ;
  }

  /* Not indexed, so use SET_COLOR */
  GET_CMD(color, state, GIPA2_SET_COLOR) ;
  NOP(color->nop1) ;
  NOP(color->nop2) ;
  color->bg = GIPA2_COLOR_NOBG ;
  rgbToCmyk(cmyk, state->DL_color) ;

  if (state->Kdither) {
    /* predefined/preloaded cells - just address them directly */
    color->cfore = state->Cdither + cmyk[0] *
                     HWA1_CELL_SIZE(hwa_colors->Cwidth, hwa_colors->Cheight) ;
    color->mfore = state->Mdither + cmyk[1] *
                     HWA1_CELL_SIZE(hwa_colors->Mwidth, hwa_colors->Mheight) ;
    color->yfore = state->Ydither + cmyk[2] *
                     HWA1_CELL_SIZE(hwa_colors->Ywidth, hwa_colors->Yheight) ;
    color->kfore = state->Kdither + cmyk[3] *
                     HWA1_CELL_SIZE(hwa_colors->Kwidth, hwa_colors->Kheight) ;

    return ;
  }

  /* Cells must be created on the fly, from the threshold data or out of
   * thin air. */
  color->cfore = PTR32(hwa1_find_asset(state, CELL_C, cmyk[0])) ;
  color->mfore = PTR32(hwa1_find_asset(state, CELL_M, cmyk[1])) ;
  color->yfore = PTR32(hwa1_find_asset(state, CELL_Y, cmyk[2])) ;
  color->kfore = PTR32(hwa1_find_asset(state, CELL_K, cmyk[3])) ;

  if (color->cfore && color->mfore && color->yfore && color->kfore)
    return ;

  if (hwa_colors) {
    /* Include cells on the fly */
    if (!color->cfore) {
      size_t size = HWA1_CELL_SIZE(hwa_colors->Cwidth, hwa_colors->Cheight) ;
      ptr32 cfore = hwa1_add_asset(state, ITSA_CELL, CELL_C, size, cmyk[0]) ;
      HqMemCpy(cfore, hwa_colors->Cdata + cmyk[0], size) ;
      color->cfore = PTR32(cfore) ;
    }
    if (!color->mfore) {
      size_t size = HWA1_CELL_SIZE(hwa_colors->Mwidth, hwa_colors->Mheight) ;
      ptr32 mfore = hwa1_add_asset(state, ITSA_CELL, CELL_M, size, cmyk[1]) ;
      HqMemCpy(mfore, hwa_colors->Mdata + cmyk[1], size) ;
      color->mfore = PTR32(mfore) ;
    }
    if (!color->yfore) {
      size_t size = HWA1_CELL_SIZE(hwa_colors->Ywidth, hwa_colors->Yheight) ;
      ptr32 yfore = hwa1_add_asset(state, ITSA_CELL, CELL_Y, size, cmyk[2]) ;
      HqMemCpy(yfore, hwa_colors->Ydata + cmyk[2], size) ;
      color->yfore = PTR32(yfore) ;
    }
    if (!color->kfore) {
      size_t size = HWA1_CELL_SIZE(hwa_colors->Kwidth, hwa_colors->Kheight) ;
      ptr32 kfore = hwa1_add_asset(state, ITSA_CELL, CELL_K, size, cmyk[3]) ;
      HqMemCpy(kfore, hwa_colors->Kdata + cmyk[3], size) ;
      color->kfore = PTR32(kfore) ;
    }
    return ;
  }

#ifdef HWA1_USE_THRESHOLDS
  if (hwa_dither) {
    /* Create the missing cells from the threshold data */
    if (!color->cfore)
      color->cfore = PTR32(hwa1_cell(state, hwa_dither->Cdata, hwa_dither->Cwidth,
                               hwa_dither->Cheight, CELL_C, cmyk[0], 4)) ;
    if (!color->mfore)
      color->mfore = PTR32(hwa1_cell(state, hwa_dither->Mdata, hwa_dither->Mwidth,
                               hwa_dither->Mheight, CELL_M, cmyk[1], 4)) ;
    if (!color->yfore)
      color->yfore = PTR32(hwa1_cell(state, hwa_dither->Ydata, hwa_dither->Ywidth,
                               hwa_dither->Yheight, CELL_Y, cmyk[2], 4)) ;
    if (!color->kfore)
      color->kfore = PTR32(hwa1_cell(state, hwa_dither->Kdata, hwa_dither->Kwidth,
                               hwa_dither->Kheight, CELL_K, cmyk[3], 4)) ;
    return ;
  }
#endif

  /* No alternative but to create missing cells from thin air. We'll use a
   * 16x16 Bayer matrix with suitable offsetting.
   */
  if (!color->cfore)
    color->cfore = PTR32(hwa1_cell(state, Bayer16x16, 16, 16, CELL_C, cmyk[0], 0)) ;
  if (!color->mfore)
    color->mfore = PTR32(hwa1_cell(state, Bayer16x16, 16,16, CELL_M, cmyk[1], 1)) ;
  if (!color->yfore)
    color->yfore = PTR32(hwa1_cell(state, Bayer16x16, 16,16, CELL_Y, cmyk[2], 2)) ;
  if (!color->kfore)
    color->kfore = PTR32(hwa1_cell(state, Bayer16x16, 16,16, CELL_K, cmyk[3], 3)) ;
  return ;
}

/* -------------------------------------------------------------------------- */
/* Clipping commands */

/* Struct used to pass clipping data between clip_required() and
   clip_update(), and to the spancount function, abusing render_blit_t
   pointer (which is not used). */
typedef struct hwa1_clip_t {
  size_t size ;    /**< Size of clipping asset data. */
  int32 count ;    /**< How many spans per line? (0 for bitmap clip) */
  Bool twobyte ;   /**< Does it fit in a 2-byte coords/size? */
  Bool hwa1 ;      /**< Can we use HWA1 for this clip? */
} hwa1_clip_t ;

/* Helper function for counting the spans on a clip line. */
static void spancount(render_blit_t *rb, dcoord y, dcoord xs, dcoord xe)
{
  hwa1_clip_t *data = (hwa1_clip_t *)rb ;
  UNUSED_PARAM(dcoord, y) ;
  UNUSED_PARAM(dcoord, xs) ;
  UNUSED_PARAM(dcoord, xe) ;
  data->count += 1 ;
}

/** Determine the size of a clipping form and the max number of spans per line
    if represented as a polygon buffer. */
static Bool clip_as_polygons(FORM *form, dbbox_t *bbox, hwa1_clip_t *clipdata)
{
  /** \todo ajcd 2013-08-13: Not clear if 4-byte forms of clip are
      similarly limited in size. */
  if ( bbox->y1 < 0 || (bbox->y2 - bbox->y1 + 1) > 65535 ||
       bbox->x1 < 0 || (bbox->x2 - bbox->x1 + 1) > 32767 ) {
    clipdata->size = 0 ;
    clipdata->count = 0 ;
    return FALSE ;
  }

  if ( theFormT(*form) == FORMTYPE_BANDRLEENCODED ) {
    dcoord y = bbox->y1 ;
    blit_t *addr = BLIT_ADDRESS(form->addr, form->l * (y - form->hoff)) ;
    do {
      spanlist_t *spans = (spanlist_t *)addr ;
      hwa1_clip_t line = { 0, 0, TRUE, TRUE } ;
      spanlist_intersecting(spans, spancount, NULL /*white*/,
                            (void *)&line /*abuse rb for data address*/,
                            y, bbox->x1, bbox->x2, 0 /*offset*/) ;
      INLINE_MAX32(clipdata->count, line.count, clipdata->count) ;
      clipdata->twobyte &= line.twobyte ;
      clipdata->hwa1 &= line.hwa1 ;
      addr = BLIT_ADDRESS(addr, form->l) ;
    } while ( ++y <= bbox->y2 && clipdata->count <= 16 ) ;
  } else {
    dcoord y = bbox->y1 ;
    blit_t *addr = BLIT_ADDRESS(form->addr, form->l * (y - form->hoff)) ;
    do {
      hwa1_clip_t line = { 0, 0, TRUE, TRUE } ;
      bitmap_intersecting(addr, spancount, NULL /*white*/,
                          (void *)&line /*abuse rb for data address*/,
                          y, bbox->x1, bbox->x2, 0 /*offset*/) ;
      INLINE_MAX32(clipdata->count, line.count, clipdata->count) ;
      clipdata->twobyte &= line.twobyte ;
      clipdata->hwa1 &= line.hwa1 ;
      addr = BLIT_ADDRESS(addr, form->l) ;
    } while ( ++y <= bbox->y2 && clipdata->count <= 16 ) ;
  }

  if ( clipdata->count > 16 ) { /* Can't do clip form as polygon clipping */
    clipdata->size = 0 ;
    clipdata->count = 0 ;
    return FALSE ;
  } else {
    /* Round up spans to 1/2/4/8/16 */
    int32 count = clipdata->count - 1 ;
    count |= count >> 2 ;
    count |= count >> 1 ;
    clipdata->count = count = max(1, count + 1) ;
    
    /* A single span per line must be padded to two. */
    if (count == 1)
      count = 2 ;
    /* Count two coordinates of the required size (2 bytes) for each line of
       the clip. */
    clipdata->size = count * 2 * 2 * (bbox->y2 - bbox->y1 + 1) ;
    /* Add a 2-byte short to store the number of spans, so we can build the
       clip when the asset is found.
       Spanlists must be 8byte aligned, so add 8 bytes for this header. */
    clipdata->size += 8 ;
    return TRUE ;
  }
}

/* Struct used to generate polygon clip buffers, abusing the render_blit_t
   pointer (which is not used) to pass it through. */
typedef struct hwa1_clipgen_t {
  uint16 *dest ;
  int32 count ;
} hwa1_clipgen_t ;

/* Helper function for constructing a polygon clip. */
static void spangen(render_blit_t *rb, dcoord y, dcoord xs, dcoord xe)
{
  hwa1_clipgen_t *data = (hwa1_clipgen_t *)rb ;
  UNUSED_PARAM(dcoord, y) ;
  *data->dest++ = HWA16(CAST_SIGNED_TO_UINT16(xs)) ;
  *data->dest++ = HWA16(CAST_SIGNED_TO_UINT16(xe)) ;
  data->count += 1 ;
}

/** Construct a polygon buffer from part of a clip form. */
static void clip_make_polygons(uint16 *dest, FORM *form,
                               const dbbox_t *bbox, int32 count)
{
  hwa1_clipgen_t line ;

  /* We need to save the span count in case we find the asset again. */
  *dest++ = CAST_SIGNED_TO_UINT16(count) ;
  *dest++ = 0 ;
  *dest++ = 0 ;
  *dest++ = 0 ; /* Pad to 8 byte alignment */
  line.dest = dest ;

  /* One per line must be padded to two. */
  if (count == 1)
    count = 2 ;

  if ( theFormT(*form) == FORMTYPE_BANDRLEENCODED ) {
    dcoord y = bbox->y1 ;
    blit_t *addr = BLIT_ADDRESS(form->addr, form->l * (y - form->hoff)) ;
    do {
      spanlist_t *spans = (spanlist_t *)addr ;
      line.count = 0 ;
      spanlist_intersecting(spans, spangen, NULL /*white*/,
                            (void *)&line /*abuse rb for data address*/,
                            y, bbox->x1, bbox->x2, 0 /*offset*/) ;
      /* Fill remainder of line with NOPs (0,0). */
      /** \todo ajcd 2013-08-13: This can also be read as requiring x2 < x1. */
      while ( line.count < count ) {
        *line.dest++ = 0 ;
        *line.dest++ = 0 ;
        ++line.count ;
      }
      addr = BLIT_ADDRESS(addr, form->l) ;
    } while ( ++y <= bbox->y2 ) ;
  } else {
    dcoord y = bbox->y1 ;
    blit_t *addr = BLIT_ADDRESS(form->addr, form->l * (y - form->hoff)) ;
    do {
      line.count = 0 ;
      bitmap_intersecting(addr, spangen, NULL /*white*/,
                          (void *)&line /*abuse rb for data address*/,
                          y, bbox->x1, bbox->x2, 0 /*offset*/) ;
      addr = BLIT_ADDRESS(addr, form->l) ;
      /* Fill remainder of line with NOPs (0,0). */
      /** \todo ajcd 2013-08-13: This can also be read as requiring x2 < x1. */
      while ( line.count < count ) {
        *line.dest++ = 0 ;
        *line.dest++ = 0 ;
        ++line.count ;
      }
    } while ( ++y <= bbox->y2 ) ;
  }
}

/** Determine the size of a clipping form and the max number of spans per line
    if represented as a bitmap. */
static Bool clip_as_bitmap(dbbox_t *bbox, hwa1_clip_t *clipdata)
{
  /* HWA1 clip bitmaps are 64-bit aligned. Get the exclusive left/right
     coords, convert to bytes. */
  dcoord x1 = (bbox->x1 & ~63) ;
  dcoord x2 = (bbox->x2 | 63) + 1 ;
  size_t linebytes = (x2 - x1) >> 3 ;
  clipdata->size = linebytes * (bbox->y2 - bbox->y1 + 1) ;
  clipdata->count = 0 ;
  if ( (clipdata->size >> 3) > 65535 )
    clipdata->twobyte = FALSE ;
  return TRUE ;
}

/** Extract a bitmap from part of a clip form. */
static void clip_make_bitmap(blit_t *dest, FORM *clipform, const dbbox_t *bbox)
{
  dcoord x1 = (bbox->x1 & ~63) ;
  dcoord x2 = (bbox->x2 | 63) ;
  dcoord y ;
  blit_t *src = BLIT_ADDRESS(clipform->addr,
                             (bbox->y1 - clipform->hoff) * clipform->l) ;

  if ( theFormT(*clipform) == FORMTYPE_BANDRLEENCODED ) {
    render_state_t rs_mask ;
    blit_chain_t mask_blits ;
    render_forms_t mask_forms ;
    FORM bitmap ;

    bitmap.type = FORMTYPE_BANDBITMAP ;
    bitmap.addr = dest ;
    bitmap.w = x2 - x1 + 1 ;
    bitmap.h = bitmap.rh = bbox->y2 - bbox->y1 + 1 ;
    bitmap.l = (x2 - x1 + 1) >> 3 ;
    bitmap.size = bitmap.h * bitmap.l ;
    bitmap.hoff = 0 ;

    /** \todo ajcd 2013-08-13: This should be unified with
        bandrleencoded_to_bitmap(), allowing a sub-bbox to be read from a
        form and written to another form with an offset (or the same
        form). */
    render_state_mask(&rs_mask, &mask_blits, &mask_forms, &invalid_surface,
                      &bitmap) ;
    rs_mask.ri.rb.ylineaddr = dest ;

    /* The clip box is set to the input coordinate space used by bitfill0/1,
       which excludes x_sep_position. spanlist_intersecting doesn't use the
       clip box, but the bitblits may assert it. */
    bbox_store(&rs_mask.ri.clip, 0, 0, bitmap.w - 1, bitmap.h - 1) ;

    for ( y = 0 ; y < bitmap.h ; ++y ) {
      spanlist_intersecting((spanlist_t *)src, bitfill1, bitfill0,
                            &rs_mask.ri.rb,
                            y, 0, bitmap.w - 1, x1) ;
      src = BLIT_ADDRESS(src, clipform->l) ;
      rs_mask.ri.rb.ylineaddr = BLIT_ADDRESS(rs_mask.ri.rb.ylineaddr,
                                             bitmap.l) ;
    }
  } else {
    /* Copy a chunk out of a bitmap mask. */
    int32 linesize = (x2 - x1 + 1) >> 3 ;

    /* Include the x1 word offset into the source, so we can just copy
       chunks of memory across. */
    src = BLIT_ADDRESS(src, BLIT_OFFSET(x1)) ;
    y = bbox->y2 - bbox->y1 + 1 ;
    do {
      HqMemCpy(dest, src, linesize) ;
      src = BLIT_ADDRESS(src, clipform->l) ;
      dest = BLIT_ADDRESS(dest, linesize) ;
    } while ( --y ) ;
  }
}


/* If we need to update the clip, return the number of bytes required */
static size_t clip_required(render_blit_t *rb, hwa1_state_t *state,
                            Bool bmclip, hwa1_clip_t *clipdata)
{
  const render_info_t *p_ri = rb->p_ri ;
  size_t required = 0 ;
  dbbox_t rectclip = state->rectclip ; /* current HWA clip */

  /* Complex clipping first. */
  if ( rb->clipmode == BLT_CLP_COMPLEX ) {
    dbbox_t dl_clipbox = state->DL_clipbox ; /* top complex clip size */
    int32 dl_clipid = state->DL_clipid ; /* top complex clip ID */

    /* Detect maskedimage clipping. This needs to be handled as a special case,
       there is no clipid that uniquely identifies this image section. */
    /** \todo ajcd 2013-08-12: Fix masked image clipping to go through the
        clip surface properly. Maybe store a clipid in masked image data to
        use for clipping. */
    if ( rb->clipform == &p_ri->p_rs->forms->maskedimageform ) {
      dl_clipid = -1 ; /* Artificial clipid won't match any real one,
                          including the invalid ID. */
      dl_clipbox = p_ri->clip ;
    }
    if ( dl_clipid != state->HWA_clipid ) {
      /*uint16 *data ;*/
      /* Get required size for dl_clipbox subset of current clip mask,
         expressed as polygons, or as a bitmap if polygons don't work and
         bmclip is TRUE. We don't have a pointer for the clip object, we use
         the clip ID to distinguish instances. Don't search for clip assets
         for masked image clips or image bitmap clips, they're too specific
         to that instance. */
      if ( /*(!bmclip && dl_clipid > 0 &&
            (data = hwa1_find_asset(state, CLIP_ASSET, dl_clipid)) != NULL &&
            (clipdata->count = data[0], data+=4, TRUE)) ||*/
           clip_as_polygons(rb->clipform, &dl_clipbox, clipdata) ||
           (bmclip && clip_as_bitmap(&dl_clipbox, clipdata)) ) {
        HQASSERT(clipdata->twobyte, "NYI 4-byte clip data") ;
        required += sizeof(gipa2_set_clip_2) + clipdata->size ;
        rectclip = dl_clipbox ;
      } else {
        HQFAIL("Can't handle clip either as polygons or as bitmap") ;
        /** \todo ajcd 2013-08-13: Fall back to use in-RIP clipping, so reset
            HWA1 clip. */
        clipdata->hwa1 = FALSE ;
        required += sizeof(gipa2_reset_clip) ;
        rectclip = state->bandclip ;
      }
    }
  } else if ( state->HWA_clipid != SURFACE_CLIP_INVALID ) {
    /* HWA previously had a complex clip, so it needs a reset. */
    required += sizeof(gipa2_reset_clip) ;
    rectclip = state->bandclip ;
  }

  /* Then an optional rectclip on top of it. */
  if ( !bbox_equal(&rectclip, &p_ri->clip) ) {
    /** \todo ajcd 2013-08-13: Use gipa2_set_clip_2 if appropriate. */
    required += sizeof(gipa2_set_clip_4) ;
  }

  return required ;
}

/* Add a complex clip and a rect clip command if we need to. */
static void clip_update(render_blit_t *rb, hwa1_state_t * state,
                        hwa1_clip_t *clipdata)
{
  const render_info_t *p_ri = rb->p_ri ;

  /* Complex clipping first. */
  if ( rb->clipmode == BLT_CLP_COMPLEX ) {
    dbbox_t dl_clipbox = state->DL_clipbox ; /* top complex clip size */
    int32 dl_clipid = state->DL_clipid ; /* top complex clip ID */

    /* Detect maskedimage clipping. This needs to be handled as a special case,
       there is no clipid that uniquely identifies this image section. */
    /** \todo ajcd 2013-08-12: Fix masked image clipping to go through the
        clip surface properly. Maybe store a clipid in masked image data to
        use for clipping. */
    if ( rb->clipform == &p_ri->p_rs->forms->maskedimageform ) {
      dl_clipid = -1 ; /* Artificial clipid won't match any real one,
                          including invalid ID. */
      dl_clipbox = p_ri->clip ;
    }
    if ( dl_clipid != state->HWA_clipid ) {
      if ( clipdata->hwa1 ) {
        gipa2_set_clip_2 *clip ;
        HQASSERT(clipdata->twobyte, "NYI 4-byte clip data") ;
        GET_CMD(clip, state, GIPA2_SET_CLIP) ;
        NOP(clip->nop1) ;
        NOP(clip->nop2) ;
        if ( clipdata->count > 0 ) {
          uint16 *data = NULL ;
          /* Clips can't be reused from band to band, so no point... */
          /* data = hwa1_find_asset(state, CLIP_ASSET, dl_clipid) ; */
          if ( data == NULL ) {
            HQASSERT(clipdata->size > 0, "No size for clip asset") ;
            data = hwa1_add_asset(state, ITSA_CLIP, CLIP_ASSET, clipdata->size,
                                  dl_clipid) ;
            HQASSERT(data, "Couldn't create polygon clip") ;
#           ifdef HWA1_METRICS
              hwa1_metrics.opcodes[GIPA2_SET_CLIP].assets += clipdata->size ;
#           endif
            clip_make_polygons(data, rb->clipform, &dl_clipbox,
                               clipdata->count) ;
          }
          /* Data starts with 4 shorts. First is span count per line. */
          clip->limit = highest_bit_set_in_byte[data[0]] ;
          data += 4 ; /* First 8 bytes are admin */

          clip->flags = GIPA2_CLIP_RUN|GIPA2_COORDS_ARE_2 ;
          clip->x = HWA16((uint16)dl_clipbox.x1) ;
          clip->y = HWA16((uint16)dl_clipbox.y1) ;
          clip->width = HWA16((uint16)(dl_clipbox.x2 - dl_clipbox.x1 + 1)) ;
          clip->height = HWA16((uint16)(dl_clipbox.y2 - dl_clipbox.y1 + 1)) ;
          clip->data = PTR32(data) ;
        } else {
          blit_t *data ;

          HQASSERT(clipdata->size > 0, "No size for clip asset") ;

          /* Give bitmap clips an invalid ID so we don't find them again.
             They're too specific to a particular masked image. */
          data = hwa1_add_asset(state, ITSA_CLIP, CLIP_ASSET, clipdata->size,
                                SURFACE_CLIP_INVALID) ;
          HQASSERT(data, "Couldn't create bitmask clip") ;
#         ifdef HWA1_METRICS
            hwa1_metrics.opcodes[GIPA2_SET_CLIP].assets += clipdata->size ;
#         endif
          clip_make_bitmap(data, rb->clipform, &dl_clipbox) ;

          clip->flags = GIPA2_CLIP_BITMAP ;
          /** \todo ajcd 2013-08-13: Not clear if the size mentioned is the
              line size or the whole clipmap size in words. Assume the line
              size here. */
          clip->width = HWA16(CAST_SIGNED_TO_UINT16(((dl_clipbox.x2 | 63) -
                               (dl_clipbox.x1 & ~63) + 1) >> 6)) ;
          clip->data = PTR32(data) ;
        }
        /* Deliberately don't match masked image faux id. */
        state->HWA_clipid = dl_clipid != -1 ? dl_clipid : -2 ;
        state->rectclip = dl_clipbox ;
      } else {
        /* We couldn't do clipping with HWA1. */
        gipa2_reset_clip *resetclip ;
        GET_CMD(resetclip, state, GIPA2_RESET_CLIP) ;
        NOP(resetclip->nop1) ;
        NOP(resetclip->nop2) ;
        NOP(resetclip->nop3) ;
        state->HWA_clipid = SURFACE_CLIP_INVALID ;
        state->rectclip = state->bandclip ;
      }
    }
  } else if ( state->HWA_clipid != SURFACE_CLIP_INVALID ) {
    /* HWA previously had a complex clip, so it needs a reset. */
    gipa2_reset_clip *resetclip ;
    GET_CMD(resetclip, state, GIPA2_RESET_CLIP) ;
    NOP(resetclip->nop1) ;
    NOP(resetclip->nop2) ;
    NOP(resetclip->nop3) ;
    state->HWA_clipid = SURFACE_CLIP_INVALID ;
    state->rectclip = state->bandclip ;
  }

  /* Then an optional rectclip on top of it. */
  /* TODO FIXME @@@@ sab 20141013: This rectangular clip ought to have been
   * taken into account during production of the complex clip - it can't be
   * applied "on top" of it. Disabled for now. */
  if (FALSE && !bbox_equal(&state->rectclip, &p_ri->clip) ) {
    gipa2_set_clip_4 *clip ;
    GET_CMD(clip, state, GIPA2_SET_CLIP) ;
    NOP(clip->nop1) ;
    NOP(clip->nop2) ;
    clip->limit = 0 ;
    clip->flags = GIPA2_CLIP_RECT|GIPA2_COORDS_ARE_4 ;
    clip->x = HWA32(p_ri->clip.x1) ;
    clip->y = HWA32(p_ri->clip.y1) ;
    clip->width = HWA32(p_ri->clip.x2 - p_ri->clip.x1 + 1) ;
    clip->height = HWA32(p_ri->clip.y2 - p_ri->clip.y1 + 1) ;
  }
}

/* ========================================================================== */
/** Render preparation function for HWA1 quantises current color. */
static surface_prepare_t hwa1_render_prepare(surface_handle_t handle,
                                             render_info_t *p_ri)
{
  hwa1_state_t *state = &handle.band->state ;
  LISTOBJECT *lobj ;
  STATEOBJECT *objstate ;
  PclAttrib *attrib ;
  const render_state_t *p_rs ;
  size_t required = 0 ;
  blit_color_t *color ;
  hwa1_clip_t clipdata = { 0, 0, TRUE, TRUE } ; /* size, count, twobyte, hwa */

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;
  HQASSERT(p_ri, "No render info") ;
  p_rs = p_ri->p_rs ;

  /* Give up now if stuff has gone wrong */
  if (state->error != HWA1_OK)
    return SURFACE_PREPARE_FAIL ;

  /* Grab any PCL state we need for the object. The erase object will have
     pclAttrib NULL. */
  lobj = p_ri->lobj ;
  objstate = lobj->objectstate ;
  if ( (attrib = objstate->pclAttrib) != NULL ) {
    /* We're going to map ROP3s that use texture onto ROP2s that use source.
       By this time, we've made sure that the ROPs coming through only rely
       on at most one of texture OR source. The RIP will have put the T or S
       color in the blit color when rendering, so we don't need to worry
       about it further. */
    state->DL_ROP2 = ROP2[attrib->rop] ;
    HQASSERT(state->DL_ROP2 != ROP2_INVALID, "Invalid ROP2") ;

    /* The HWA1 brush can only be used for black/white patterns */
    if ( attrib->patternColors == PCL_PATTERN_BLACK_AND_WHITE ) {
      state->DL_pattern = attrib->dlPattern ;
      state->DL_pTrans = attrib->patternTransparent ;
    } /* else uses pattern blit layer */

    state->sTrans = attrib->sourceTransparent ;

#ifdef ASSERT_BUILD
    switch (state->DL_ROP2) {
      /* These are supported, but unexpected - not an error, but interesting */
    case ROP2_Wht:
    case ROP2_DSa:  /* & DTa */
    case ROP2_SDna: /* & TDna */
    case ROP2_DSna: /* & DTna */
    case ROP2_D:
    case ROP2_DSo:  /* & DTo */
    case ROP2_DSon: /* & DTon */
    case ROP2_DSxn: /* & TDxn */
    case ROP2_Dn:
      HQFAIL("Unexpected ROP type for HWA1") ;
      break ;
    }
#endif
  }

  color = p_ri->rb.color ;
  color->packed.channels.words[0] = 0 ; /* We may only set one channel. */
  blit_color_quantise(color) ;
  blit_color_pack(color) ;

  state->DL_color = color->packed.channels.words[0];

  /* Set the ROP regardless of object type. For non-images, set the brush and
     color too. This applies for all of the successive spans/blocks/chars. */

  /* If the ROP isn't correct, we'll need to change it */
  required += ROP2_required(state) ;

  /* If the brush isn't correct, we'll need to change it */
  required += brush_required(state) ;

  /* If we're going to use STRETCHBLT, set the source transparent color. */
  /** \todo ajcd 2013-08-12: probably force sTrans true for RENDER_char, so
      we don't blat the white bits. Unless we want them, of course. */
  if ( lobj->opcode == RENDER_image || lobj->opcode == RENDER_char ) {
    required += pcolor_required(state) ;
  }

  /* Defer clipping for images until masking done. */
  if ( lobj->opcode != RENDER_image ) {
    required += clip_required(&p_ri->rb, state, FALSE /*polygons*/,
                              &clipdata) ;
    /** \todo ajcd 2013-08-13: Do something if we can't handle the clip
        using HWA1. */
    HQASSERT(clipdata.hwa1, "Can't handle clip using HWA1") ;
  }

  /* We don't actually care if this is a new buffer */
  (void) wont_fit(state, required) ;

  /* Set the ROP if we need to */
  ROP2_update(state) ;

  /* Set the brush if we need to */
  brush_update(p_ri->p_rs->page, state) ;

  if ( lobj->opcode == RENDER_image || lobj->opcode == RENDER_char ) {
    pcolor_update(state) ;
  }

  /* Defer clipping for images until masking done. */
  if ( lobj->opcode != RENDER_image ) {
    clip_update(&p_ri->rb, state, &clipdata) ;
  }

  return SURFACE_PREPARE_OK;
}

/* ========================================================================== */
/* Erase */

static void hwa1_erase(render_blit_t *rb, FORM *form)
{
  /* Don't think we need to erase, it's implicit. */

  UNUSED_PARAM(render_blit_t *, rb) ;
  UNUSED_PARAM(FORM *, form) ;
#if 0
  const render_info_t * p_ri  = rb->p_ri ;
  const render_state_t * p_rs = p_ri->p_rs ;
  hwa1_state_t *state = &p_rs->surface_handle.band->state ;

  gipa2_band_init * band ;
  gipa2_draw_run_repeat_4 * erase ;
  size_t required = sizeof(*band) + sizeof(*erase) ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  /* Initialise current clip and band clip to (unclipped) erase size. */
  state->rectclip = state->bandclip = p_ri->clip ;

  /* An erase is probably as simple as RUN_REPEAT with ROP2_Wht */
  state->DL_ROP2 = ROP2_Wht ;
  required += ROP2_required(state) ;

  /* We don't actually care if this is a new buffer */
  (void) wont_fit(state, required) ;

  /* Create the GIPA2_BAND_INIT. */
  GET_CMD(band, state, GIPA2_BAND_INIT) ;
  NOP(band->nop1) ;
  NOP(band->nop2) ;
  NOP(band->nop3) ;
  band->planes   = GIPA2_CMYK ;  /* FIXME: GIPA2_K for grey */
  band->bitdepth = GIPA2_1BIT ;  /* FIXME: GIPA2_2BIT */
  band->pixels   = HWA32(form->w) ;    /* FIXME: or what? */
  band->bytes    = HWA32((band->pixels + 63) >> 3) ;        /* 8 byte aligned */
  band->rows     = HWA32(form->rh) ;
  band->pagewidth = HWA32(form->w) ;
  band->pageheight = HWA32(form->h) ;
  band->offset   = HWA32(form->hoff) ;
  band->kband    =
    band->cband  =
    band->mband  =
    band->yband  = PTR32(form->addr) ;         /* FIXME: yeah */

  /** \todo ajcd 2013-08-12: Do we even need to blat white, or does the band
      init cover it? */

  /* Set the ROP if we need to */
  ROP2_update(state) ;

  GET_CMD(erase, state, GIPA2_DRAW_RUN_REPEAT) ;
  NOP(erase->nop1) ;
  erase->coords = GIPA2_COORDS_ARE_4 ;
  erase->planes = GIPA2_CMYK ;
  erase->y      = 0 ;
  erase->x0     = 0 ;
  erase->x1     = HWA32(form->w - 1) ;
  erase->rows   = HWA32(form->rh) ;
#endif
}

/* ========================================================================== */
/* Spans */


static void hwa1_repeat4(hwa1_state_t * state,
                         uint32 y, uint32 x0, uint32 x1, uint32 rows)
{
  gipa2_draw_run_repeat_4 * repeat = (gipa2_draw_run_repeat_4 *) state->prev ;
  /* Can we update the previous run repeat? */
  if (repeat && repeat->opcode == GIPA2_DRAW_RUN_REPEAT &&
      repeat->coords == GIPA2_COORDS_ARE_4 &&
      x0 == HWA32(repeat->x0) && x1 == HWA32(repeat->x1) &&
      y == HWA32(repeat->y) + HWA32(repeat->rows)) {
    repeat->rows = HWA32(rows + HWA32(repeat->rows)) ;
    return ;
  }
  /* Add a new one */
  GET_CMD(repeat, state, GIPA2_DRAW_RUN_REPEAT) ;
  NOP(repeat->nop1) ;
  repeat->coords = GIPA2_COORDS_ARE_4 ;
  repeat->planes = GIPA2_CMYK ;
  repeat->y      = HWA32(y) ;
  repeat->x0     = HWA32(x0) ;
  repeat->x1     = HWA32(x1) ;
  repeat->rows   = HWA32(rows) ;
}

static void hwa1_repeat2(hwa1_state_t * state,
                         uint16 y, uint16 x0, uint16 x1, uint16 rows)
{
  gipa2_draw_run_repeat_2 * repeat = (gipa2_draw_run_repeat_2 *) state->prev ;
  /* Can we update the previous run repeat? */
  if (repeat && repeat->opcode == GIPA2_DRAW_RUN_REPEAT &&
      repeat->coords == GIPA2_COORDS_ARE_2 &&
      x0 == HWA16(repeat->x0) && x1 == HWA16(repeat->x1) &&
      y == HWA16(repeat->y) + HWA16(repeat->rows)) {
    repeat->rows = HWA16(rows + HWA16(repeat->rows)) ;
    return ;
  }
  /* Add a new one */
  GET_CMD(repeat, state, GIPA2_DRAW_RUN_REPEAT) ;
  NOP(repeat->nop1) ;
  repeat->coords = GIPA2_COORDS_ARE_2 ;
  repeat->planes = GIPA2_CMYK ;
  repeat->y      = HWA16(y) ;
  repeat->x0     = HWA16(x0) ;
  repeat->x1     = HWA16(x1) ;
  repeat->rows   = HWA16(rows) ;
}


static void hwa1_span(render_blit_t *rb, dcoord y, dcoord xs, dcoord xe)
{
  /* FIXME: We really don't want to be dealing with individual spans - we want
     to do them all in one go. If we can't do that within the blit stack then
     we'll have to update the previous command's scanline definition where
     possible.

     However, faffing with the scanline definition is a major pain because
     assets are at the *end* of the buffer (so we'd have to prefix each new
     span, updating the asset header), BUT the whole scanline definition needs
     to be 8byte aligned, even though each span is 6 or 12 bytes (three int16s
     or int32s - y,x0,x1). :-[

     Consequently we'd have to add blocks of spans (4*6 or 2*12) to maintain
     the alignment requirement, setting the unused spans to x0 > x1 to suppress
     output of that span (see page 61). If we were using DLDE compressed spans
     it's even more complicated. And we'd also have to deal with the buffer
     filling up and hence having to create a new command in the new buffer.

     It would be good if we could do all the spans of a polygon in one go!
   */

  /** \todo ajcd 2013-08-11: use a single run repeat as a proxy for spans. */

  const render_info_t * p_ri  = rb->p_ri ;
  const render_state_t * p_rs = p_ri->p_rs ;
  hwa1_state_t *state = &p_rs->surface_handle.band->state ;
  size_t required = 0 ;
  Bool fourbyte ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  /** \todo ajcd 2013-08-13: Not sure of the exact limitations on 2-byte
      format, the doc is not clear. These are guesses based on other
      operations. */
  fourbyte = (y < 0 || y > 32767 || xs < 0 || xe > 32767) ;
  if (fourbyte)
    required += sizeof(gipa2_draw_run_repeat_4) ;
  else
    required += sizeof(gipa2_draw_run_repeat_2) ;

  /* If the colour isn't correct, we'll need to change it */
  required += color_required(state, FALSE) ;

  /* We don't actually care if this is a new buffer */
  (void) wont_fit(state, required) ;

  /* Set the color if we need to */
  color_update(state, FALSE) ;

  /** \todo ajcd 2013-08-13: If we can't do HWA1 clipping and the clipmode is
      complex, convert to span in-RIP. */
  if (fourbyte)
    hwa1_repeat4(state, (uint32)y, (uint32)xs, (uint32)xe, 1) ;
  else
    hwa1_repeat2(state, (uint16)y, (uint16)xs, (uint16)xe, 1) ;

}

static void hwa1_clipspan(render_blit_t *rb,
                          dcoord y, register dcoord xs, register dcoord xe)
{
  BITCLIP_ASSERT(rb, xs, xe, y, y, "hwa1_clipspan" ) ;

  bitclipn(rb, y, xs, xe, hwa1_span) ;
}

/* ========================================================================== */
/* Blocks */

static void hwa1_block(render_blit_t *rb,
                       dcoord ys, dcoord ye, dcoord xs, dcoord xe)
{
  const render_info_t * p_ri  = rb->p_ri ;
  const render_state_t * p_rs = p_ri->p_rs ;
  hwa1_state_t *state = &p_rs->surface_handle.band->state ;
  size_t required = 0 ;
  Bool fourbyte ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  /** \todo ajcd 2013-08-13: Not sure of the exact limitations on 2-byte
      format, the doc is not clear. These are guesses based on other
      operations. */
  fourbyte = (ys < 0 || ye > 32767 || xs < 0 || xe > 32767) ;
  if (fourbyte)
    required += sizeof(gipa2_draw_run_repeat_4) ;
  else
    required += sizeof(gipa2_draw_run_repeat_2) ;

  /* If the colour isn't correct, we'll need to change it */
  required += color_required(state, FALSE) ;

  /* We don't actually care if this is a new buffer */
  (void) wont_fit(state, required) ;

  /* Set the color if we need to */
  color_update(state, FALSE) ;

  /** \todo ajcd 2013-08-13: If we can't to HWA1 clipping and the clipmode is
      complex, convert to span in-RIP. */
  if (fourbyte)
    hwa1_repeat4(state, (uint32)ys, (uint32)xs, (uint32)xe, (uint32)(ye-ys+1)) ;
  else
    hwa1_repeat2(state, (uint16)ys, (uint16)xs, (uint16)xe, (uint16)(ye-ys+1)) ;
}

/* ========================================================================== */
/* Chars */

static void hwa1_char(render_blit_t * rb, FORM * form, dcoord x, dcoord y)
{
  const render_info_t * p_ri  = rb->p_ri ;
  const render_state_t * p_rs = p_ri->p_rs ;
  hwa1_state_t *state = &p_rs->surface_handle.band->state ;

  dcoord x1c, y1c, x2c, y2c ;
  blit_t * dest = 0 ;
  int32  w, h, l ;
  size_t required = 0, assetsize = 0 ;
  uint8 run_coords = GIPA2_COORDS_ARE_2 ;
  int32 type = 1 ; /* 0 = image asset, 1 = scanline asset */

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  bbox_load(&rb->p_ri->clip, x1c, y1c, x2c, y2c) ;

  /* Extract all the form info. */
  w = form->w ;
  h = form->h ;
  l = form->l >> BLIT_SHIFT_BYTES ;

  /* are we off the edge of the character because of clipping? */
  if ( x > x2c || y > y2c || x + w <= x1c || y + h <= y1c )
    return ;

  if (form->type < FORMTYPE_CACHERLE1) {
    /* It's a bitimage - for now use STRETCHBLT as before, though we intend to
     * detect spans as for the RLE case below. */

    /* HWA1 only does word-aligned BITBLTs, so do we buffer up to 32 versions of
     * a common char just so we can BITBLT, or do we use STRETCHBLT to minimise
     * buffer usage at the expense of rendering time... assuming there is a
     * greater cost to STRETCHBLT than BITBLT?
     *
     * FIXME: For now, use STRETCHBLT. */

    HQASSERT(form->type == FORMTYPE_CACHEBITMAPTORLE ||
      form->type == FORMTYPE_CACHEBITMAP ||
      form->type == FORMTYPE_BANDBITMAP ||
      form->type == FORMTYPE_HALFTONEBITMAP, /* Pattern screens */
      "Char form is not bitmap") ;

    required = sizeof(gipa2_draw_stretchblt) ;

    dest = hwa1_find_asset(state, form, 0) ;
    if (!dest) {
      assetsize = ((form->size + 7) & ~7) ;
      required += assetsize + sizeof(assetlist) ;
      type = 0 ;
    }

  } else {
    /* It's RLE, which we'll represent as a DRAW_RUN. Regrettably, RUN doesn't
     * have an X,Y position so CAN'T reuse scanline sets in different places.
     * This is a significant restriction as the STRETCHBLT case CAN reuse a
     * single asset. The compromise is speed versus size, as always. */

    dcoord rows = form->h ;
    RLECACHE_LINE_READ_STATE state ;

    state.next_line = (uint8*) form->addr ;

    /* Analyse the RLE. We'll count up assetsize as though the coords are
     * 2 byte, and double it at the end if they're actually 4 byte. */
    assetsize = 2 ; /* terminator */

    while (rows > 0) {
      int32 span[2] ;
      int spans = 0 ;

      rlecache_line_read_init(&state, form, state.next_line);
      while (!state.line_finished) {
        rlecache_get_span_pair(&state, span) ;
        ++spans ;
      }
      /* Each span in the uncompressed 16bit representation is 6 bytes long */
      assetsize += 6 * spans * rows ;
      rows -= state.row_height ;
    }
    /* Here x + form->w is the max X coordinate and y + form->h is max Y coord.
     * If either of these won't fit in an int16, bump up to 32bit. */
    if (x < -32768 || y < 0 || (x + w) > 32767 || (y + h) > 32767) {
      run_coords = GIPA2_COORDS_ARE_4 ;
      assetsize *= 2 ;
    }
    required = sizeof(gipa2_draw_run_yclip) + assetsize + sizeof(assetlist) ;
  }

  /* If the colour isn't correct, we'll need to change it */
  required += color_required(state, TRUE) ;

  if (wont_fit(state, required)) {
    /* New buffer now, so do need to add this asset (we assume it will fit) */
    dest = NULL ;
  }

  /* Add the asset if we need to */
  if (!dest) {
    dest = hwa1_add_asset(state, ITSA_CHAR, form, assetsize, type) ;
    if (!dest) {
      /* Now we're stuck */
      HQFAIL("Can't add char asset") ;
      state->error = HWA1_ERROR_OVERRUN ;
      return ;
    }
#   ifdef HWA1_METRICS
      hwa1_metrics.opcodes[GIPA2_DRAW_STRETCHBLT].assets += assetsize ;
#   endif

    if (form->type < FORMTYPE_CACHERLE1) {
      /* A bitimage */
      HqMemCpy(dest, form->addr, form->size) ;
    } else {
      /* RLE - convert to spans in the format we've already decided. */

      dcoord Y = y, rows = form->h ;
      RLECACHE_LINE_READ_STATE state ;
      int16 * coord16 = (int16 *) dest ;
      int32 * coord32 = (int32 *) dest ;

      state.next_line = (uint8*)form->addr ;

      while (rows > 0) {
        dcoord sx, ex = x, r ;
        int32 span[2] ;

        rlecache_line_read_init(&state, form, state.next_line);
        while (!state.line_finished) {
          rlecache_get_span_pair(&state, span) ;
          sx = ex + span[0] ;
          ex = sx + span[1] ;

          if (run_coords == GIPA2_COORDS_ARE_2) {
            for (r = 0 ; r < state.row_height ; ++r) {
              coord16[0] = HWA16((int16) (Y + r)) ;
              coord16[1] = HWA16((int16) sx) ;
              coord16[2] = HWA16((int16) ex) ;
              coord16 += 3 ;
            }
          } else {
            for (r = 0 ; r < state.row_height ; ++r) {
              coord32[0] = HWA32(Y + r) ;
              coord32[1] = HWA32(sx) ;
              coord32[2] = HWA32(ex) ;
              coord32 += 3 ;
            }
          }
        }
        Y += state.row_height ;
        rows -= state.row_height ;
      }
      /* Terminator */
      if (run_coords == GIPA2_COORDS_ARE_2) {
        *coord16++ = -1 ;
        HQASSERT(coord16 <= (int16*)(dest + assetsize),
                 "16bit scanline overflow") ;
      } else {
        *coord32++ = -1 ;
        HQASSERT(coord32 <= (int32*)(dest + assetsize),
                 "32bit scanline overflow") ;
      }
    }
  }

  /* Set the color if we need to */
  color_update(state, TRUE) ;

  /* Now add the STRETCHBLT or DRAW_RUN command */
  if (form->type < FORMTYPE_CACHERLE1) {
    gipa2_draw_stretchblt * stretch ;

    GET_CMD(stretch, state, GIPA2_DRAW_STRETCHBLT) ;
    NOP(stretch->nop1) ;
    NOP(stretch->nop2) ;
    NOP(stretch->nop3) ;
    stretch->flags = GIPA2_IMAGE_CMYK | GIPA2_IMAGE_INDEXED ;
    if ( state->sTrans )
      stretch->flags  |= GIPA2_IMAGE_TRANS ;
    stretch->planes    = GIPA2_CMYK ;
    stretch->srcwidth  = HWA32(w) ;
    stretch->srcheight = HWA32(h) ;
    stretch->width     = HWA32(w) ;
    stretch->height    = HWA32(h) ;
    stretch->xscale    = HWA32(65536) ;
    stretch->yscale    = HWA32(65536) ;
    stretch->x         = HWA32((int32) x) ;
    stretch->y         = HWA32((int32) y) ;
    stretch->cxoffset  = 0 ;
    stretch->cyoffset  = 0 ;
    stretch->mxoffset  = 0 ;
    stretch->myoffset  = 0 ;
    stretch->yxoffset  = 0 ;
    stretch->yyoffset  = 0 ;
    stretch->kxoffset  = 0 ;
    stretch->kyoffset  = 0 ;
    stretch->data      = PTR32(dest) ;
  } else {
    /* Add a DRAW_RUN - the Y-clipped version has a smaller coordinate range. */
    if (y < -65535 || (y + h) > 65535) {
      /* Coords are too extreme for the clipped case... */
      gipa2_draw_run * run ;

      GET_CMD(run, state, GIPA2_DRAW_RUN) ;
      NOP(run->nop1) ;
      run->coords    = run_coords ;
      run->flags     = GIPA2_YCLIP | GIPA2_NOCOMP ;  /* No DLDE yet */
      run->planes    = GIPA2_CMYK ;
      run->scanlines = PTR32(dest) ;
    } else {
      gipa2_draw_run_yclip * run ;

      GET_CMD(run, state, GIPA2_DRAW_RUN) ;
      NOP(run->nop1) ;
      run->coords    = run_coords ;
      run->flags     = GIPA2_YCLIP | GIPA2_NOCOMP ;  /* No DLDE yet */
      run->planes    = GIPA2_CMYK ;
      run->scanlines = PTR32(dest) ;
      run->y0        = HWA32(y) ;
      run->y1        = HWA32(y + h) ;
    }
  }

  return ;
}

/* ========================================================================== */
/* Images */

static void hwa1_image(render_blit_t * rb,
                       imgblt_params_t * params,
                       imgblt_callback_fn * callback,
                       Bool *result)
{
  const render_info_t * p_ri  = rb->p_ri ;
  const render_state_t * p_rs = p_ri->p_rs ;
  hwa1_state_t *state = &p_rs->surface_handle.band->state ;
  size_t rowlen = params->ncols * params->expanded_comps ;
  size_t imglen = rowlen * params->nrows ;
  size_t clip_size = 0, required = 0 ;
  hwa1_clip_t clipdata = { 0, 0, TRUE, TRUE } ; /* size, count, twobyte, hwa */
  gipa2_draw_stretchblt * stretch ;
  Bool use_hwa1 ;
  dbbox_t bbox = { 0 } ;
  dcoord destwidth = 0, destheight = 0 ;
  uint8 *dest, *out, *end ;
  int xtiles = 1, ytiles = 1, xtile, ytile ;
  int nrows = params->nrows ; /* rows left to do */
  int top = 0 ;
  int clip_it = TRUE ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  /* We only expect orthogonal images for PCL. Knockouts, if they occur, can
     go to the default with all other cases... if there are any. */
  use_hwa1 = (params->orthogonal && params->type != IM_BLIT_KNOCKOUT) ;

  /* We believe that STRETCHBLT can only be used for upscaling, not downscaling,
   * so resort to software if the resolution is too high */
  if (use_hwa1) {
    bbox_intersection(&params->image->bbox, &rb->p_ri->clip, &bbox) ;
    /** \todo ajcd 2013-08-11: These are not quite right, they include the
        clipped size of the image, not the unclipped edges. */
    destwidth = bbox.x2 - bbox.x1 + 1 ;
    destheight = bbox.y2 - bbox.y1 + 1 ;
    if (destwidth < params->ncols || destheight < params->nrows)
      use_hwa1 = FALSE ;
  }

  /* If the clipping isn't correct, we'll need to change it */
  clip_size = clip_required(rb, state, use_hwa1 /*bitmap OK*/, &clipdata) ;

  /* Can we use HWA1 clip? If not, we need to fallback to the generic image
     routines to coerce the image to chars/blocks/spans. */
  if ( !clipdata.hwa1 )
    use_hwa1 = FALSE ;

  if (!use_hwa1) {
    /* Unable to use hardware, so fall back to default rendering. */
    imagebltn(rb, params, callback, result) ;
    return ;
  }

  /* We can use HWA1, off we go.
   *
   * If the image dimensions are too big we need to tile. We will need to apply
   * the above clip once for each buffer only.
   */

  if (params->ncols > 10239 || params->nrows > 16383) {
    /* We need to tile the image.
     *
     * We only need to apply the above clip once per buffer though.
     */

    /* Limit tile sizes to a multiple of 64 pixels for 1bpp 8byte alignment */
#   define XTILESIZE 10176
#   define YTILESIZE 16383
    xtiles = (params->ncols + XTILESIZE - 1) / XTILESIZE ;
    ytiles = (params->nrows + YTILESIZE - 1) / YTILESIZE ;
  }

  for (ytile = 0 ; ytile < ytiles ; ++ytile) {
    int rows = min(nrows, YTILESIZE) ; /* rows in these tiles */
    int ncols = params->ncols ; /* columns left to do */
    int left = 0 ;
    int32 width, height ;
    for (xtile = 0 ; xtile < xtiles ; ++xtile) {
      int cols = min(ncols, XTILESIZE) ; /* columns in this tile */
      int line, lines ;

      /* Size of THIS image tile */
      rowlen = cols * params->expanded_comps ;
      imglen = rows * rowlen ;

      /* Now do the tile at:left,top size:cols,rows... */
      required = sizeof(*stretch) + imglen + sizeof(assetlist) ;
      if (clip_it)
        required += clip_size ;
      if (wont_fit(state, required) && !clip_it) {
        /* Now in a new buffer, so redo the clip - normally we treat the setup
           commands and the drawing command as a single atomic unit for buffer
           fitting purposes, but for large images we should probably fit as much
           as we can in each buffer.
        */
        clip_it = TRUE ;
        required += clip_size ;
        if (wont_fit(state, required)) {
          /* Now we're really stuck */
          *result = error_handler(LIMITCHECK) ;
          return ;
        }
      }
      if (clip_it) {
        clip_update(rb, state, &clipdata) ;
        clip_it = FALSE ;
      }

      /* Add this tile as an asset */
      out = dest = hwa1_add_asset(state, ITSA_IMAGE, params->image, imglen,
                                  xtile + ytile*xtiles) ; /* tile num */
#     ifdef HWA1_METRICS
        hwa1_metrics.opcodes[GIPA2_DRAW_STRETCHBLT].assets += imglen ;
#     endif
      if (!dest) {
        /* We thought it would fit, but it didn't. Odd. */
        *result = error_handler(LIMITCHECK) ;
        return ;
      }
      end = out + imglen ;

      /* Expand this image tile */
      line = params->irow + top ; /* The next line number to extract */
      lines = rows ;              /* The number of lines left to extract */
      do {
        const void *values ;
        dcoord repeats, i ;

        if (!interrupts_clear(allow_interrupt)) {
          *result = report_interrupt(allow_interrupt);
          return ;
        }

        /* Expand a row */
        values = im_expandread(params->image->ime, params->image->ims,
                               params->expand_arg,
                               params->lcol + left, line, ncols,
                               &repeats, params->expanded_to_plane,
                               params->expanded_comps) ;
        if (values == NULL) {
          *result = FALSE ;
          return ;
        }
        if (params->drow < 0)
           repeats = 1 ;
        else if (repeats > lines)
          repeats = lines ;

        HQASSERT(repeats > 0, "repeats < 1") ;
        /* Copy this row, potentially multiple times */
        for (i = 0 ; i < repeats ; ++i) {
          HQASSERT(out + rowlen <= end, "Img overrun") ;
          if (params->dcol >= 0) {
            /* Copy row normally. */
            HqMemCpy(out, values, rowlen) ;
          } else {
            /* Reverse row order while copying */
            uint8 * start = (uint8*) values, * from, * to = out ;
            if (params->expanded_comps == 3) { /* RGB */
              for (from = start + rowlen - 3 ; from >= start ; from -= 3) {
                *to++ = from[0] ;
                *to++ = from[1] ;
                *to++ = from[2] ;
              }
            } else { /* Gray or anything unexpected */
              for (from = start + rowlen - 1 ; from >= start ; --from)
                *to++ = from[0] ;
            }
          }
          out += rowlen ;
        }

        /* Advance to next line */
        line += repeats * params->drow ;
        lines -= repeats ;
      } while (lines > 0) ;

      /* Now add the stretchblt command */
      GET_CMD(stretch, state, GIPA2_DRAW_STRETCHBLT) ;
      NOP(stretch->nop1) ;
      NOP(stretch->nop2) ;
      NOP(stretch->nop3) ;
      switch (params->expanded_comps) {
      default:
        HQFAIL("Unexpected number of image components") ;
        /* drop through into case 1 */
      case 1:
        stretch->flags = GIPA2_IMAGE_PLANES ;
        stretch->planes = GIPA2_K ;
        break ;
      case 3:
        stretch->flags = GIPA2_IMAGE_RGB ;
        stretch->planes = GIPA2_CMYK ;
        break ;
      case 4:
        stretch->flags = GIPA2_IMAGE_CMYK ;
        stretch->planes = GIPA2_CMYK ;
        break ;
      }
      if (state->sTrans)
        stretch->flags |= GIPA2_IMAGE_TRANS ;
      stretch->srcwidth  = HWA32(cols) ;
      stretch->srcheight = HWA32(rows) ;
      /* Plenty of opportunity to get this wrong TODO check this.... */
      width  = (int32)ceil((double)destwidth / params->ncols * cols) ;
      height = (int32)ceil((double)destheight / params->nrows * rows) ;
      stretch->width  = HWA32(width) ;
      stretch->height = HWA32(height) ;
      stretch->xscale = HWA32((int32)(65536.0 * width / cols + 0.5)) ;
      stretch->yscale = HWA32((int32)(65536.0 * height / rows + 0.5)) ;
      stretch->x = HWA32(bbox.x1) ;
      stretch->y = HWA32(bbox.y1) ;
      /** \todo sab 20140725: These should probably be calculated too */
      stretch->cxoffset = 0 ;
      stretch->cyoffset = 0 ;
      stretch->mxoffset = 0 ;
      stretch->myoffset = 0 ;
      stretch->yxoffset = 0 ;
      stretch->yyoffset = 0 ;
      stretch->kxoffset = 0 ;
      stretch->kyoffset = 0 ;
      stretch->data = PTR32(dest) ;
      ncols -= cols ;
      left += cols ;
    }
    nrows -= rows ;
    top += rows ;
  }

}

/* -------------------------------------------------------------------------- */

static void hwa1_deselect(surface_instance_t **instance, Bool continues)
{
  UNUSED_PARAM(surface_instance_t **, instance) ;

# ifndef HWA1_METRICS
  UNUSED_PARAM(Bool, continues) ;
# else
  if ( !continues && hwa1_metrics.total_buffers > 0 ) {
    monitorf((uint8 *)("HWA1 total buffers %u, bytes %u (commands %u, assets %u)\n"
                       "     peak page buffers %u, bytes %u (commands %u, assets %u)\n"),
             (uint32)hwa1_metrics.total_buffers,
             (uint32)hwa1_metrics.total_bytes,
             (uint32)hwa1_metrics.total_cmds,
             (uint32)hwa1_metrics.total_assets,
             (uint32)hwa1_metrics.peak_page_buffers,
             (uint32)hwa1_metrics.peak_page_bytes,
             (uint32)hwa1_metrics.peak_page_cmds,
             (uint32)hwa1_metrics.peak_page_assets) ;
    if ( FALSE ) {
      /* These stats don't make any sense unless doing per-band buffering,
         because we only flush the buffers when full, which they typically
         aren't on a single band. */
      monitorf((uint8 *)("     peak band buffers %u, bytes %u (commands %u, assets %u)\n"),
               (uint32)hwa1_metrics.peak_band_buffers,
               (uint32)hwa1_metrics.peak_band_bytes,
               (uint32)hwa1_metrics.peak_band_cmds,
               (uint32)hwa1_metrics.peak_band_assets) ;
    }
  }
# endif
}

/* -------------------------------------------------------------------------- */
/* Clip surface. Clip regeneration happens within the band context. The HWA
   clip surface uses the builtin span clip surface, but wraps functions
   around it to track the complex clips, and to extract the spans into clip
   polygon structures. */
static clip_surface_t hwa1_clip_surface = CLIP_SURFACE_INIT ;

/** The original clip surface's complex clip function. */
static Bool (*span_complex_clip)(surface_handle_t handle,
                                 int32 clipid, int32 parentid,
                                 render_blit_t *rb,
                                 surface_clip_callback_fn *callback,
                                 surface_clip_callback_t *data) ;

/** The replacement complex clip function. This tracks the current complex
    clip, and also determines its size as a polygon buffer. */
static Bool hwa1_complex_clip(surface_handle_t handle,
                              int32 clipid, int32 parentid,
                              render_blit_t *rb,
                              surface_clip_callback_fn *callback,
                              surface_clip_callback_t *data)
{
  hwa1_state_t *state = &handle.band->state ;
  Bool result ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;
  result = (*span_complex_clip)(handle, clipid, parentid, rb, callback, data) ;

  if ( result ) {
    state->DL_clipid = clipid ;
    state->DL_clipbox = rb->p_ri->clip ;
  } else {
    state->error = HWA1_ERROR_CALLBACK ;
  }

  return result ;
}

static Bool hwa1_rect_clip(surface_handle_t handle,
                           int32 rectid, int32 complexid,
                           const dbbox_t *bbox)
{
  hwa1_state_t *state = &handle.band->state ;

  UNUSED_PARAM(int32, rectid) ;
  UNUSED_PARAM(const dbbox_t *, bbox) ;

  VERIFY_OBJECT(state, HWA1_BAND_STATE_NAME) ;

  state->DL_clipid = complexid ;
  return TRUE ;
}

/* -------------------------------------------------------------------------- */

void init_hwa1(void)
{
  /** Alternative PackingUnitRequests for hwa1 surface. */
  const static sw_datum hwa1_packing[] = {
    SW_DATUM_INTEGER(0), /* Not defined */
    SW_DATUM_INTEGER(8),
#ifdef highbytefirst
    /* High byte first is the same order for any packing depth. */
    SW_DATUM_INTEGER(16),
    SW_DATUM_INTEGER(32),
#if BLIT_WIDTH_BYTES > 4
    SW_DATUM_INTEGER(64),
#endif
#endif
  } ;

  /* Pagedevice /Private dictionary for hwa1 mono surface selection. */
  const static sw_datum hwa1_private_mono[] = {
    /* Comparing integer to array auto-coerces to the length. */
    SW_DATUM_STRING("ColorChannels"), SW_DATUM_INTEGER(1),
  } ;

  /* TODO FIXME @@@@ sab 20141013: We have TWO mono definitions. One with "mono"
   * interleaving for "mono" jobs, and one for pixel interleaving for colour
   * jobs that switch into mono. Really the interleaving style ought to be
   * completely ignored when you only have 1 ColorChannels! */

  /* Pagedevice match for mono hwa1 surface. */
  const static sw_datum hwa1_dict_mono[] = {
    SW_DATUM_STRING("HWA1"), SW_DATUM_BOOLEAN(TRUE),
    SW_DATUM_STRING("Halftone"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("InterleavingStyle"), SW_DATUM_INTEGER(GUCR_INTERLEAVINGSTYLE_MONO),
    SW_DATUM_STRING("PackingUnitRequest"),
      SW_DATUM_ARRAY(&hwa1_packing[0], SW_DATA_ARRAY_LENGTH(hwa1_packing)),
    SW_DATUM_STRING("Private"),
      SW_DATUM_DICT(&hwa1_private_mono[0], SW_DATA_DICT_LENGTH(hwa1_private_mono)),
    SW_DATUM_STRING("ProcessColorModel"), SW_DATUM_STRING("DeviceGray"),
    SW_DATUM_STRING("RunLength"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("ValuesPerComponent"), SW_DATUM_INTEGER(256),
  } ;

  /* Pagedevice match for color-switched-to-mono hwa1 surface. */
  const static sw_datum hwa1_dict_mono2[] = {
    SW_DATUM_STRING("HWA1"), SW_DATUM_BOOLEAN(TRUE),
    SW_DATUM_STRING("Halftone"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("InterleavingStyle"), SW_DATUM_INTEGER(GUCR_INTERLEAVINGSTYLE_BAND),
    SW_DATUM_STRING("PackingUnitRequest"),
    SW_DATUM_ARRAY(&hwa1_packing[0], SW_DATA_ARRAY_LENGTH(hwa1_packing)),
    SW_DATUM_STRING("Private"),
    SW_DATUM_DICT(&hwa1_private_mono[0], SW_DATA_DICT_LENGTH(hwa1_private_mono)),
    SW_DATUM_STRING("ProcessColorModel"), SW_DATUM_STRING("DeviceGray"),
    SW_DATUM_STRING("RunLength"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("ValuesPerComponent"), SW_DATUM_INTEGER(256),
  } ;

  /* Pagedevice /Private dictionary for hwa1 color surface selection. */
  const static sw_datum hwa1_private_color[] = {
    /* Comparing integer to array auto-coerces to the length. */
    SW_DATUM_STRING("ColorChannels"), SW_DATUM_INTEGER(3),
  } ;

  /* Pagedevice match for color hwa1 surface. */
  const static sw_datum hwa1_dict_color[] = {
    SW_DATUM_STRING("HWA1"), SW_DATUM_BOOLEAN(TRUE),
    SW_DATUM_STRING("Halftone"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("InterleavingStyle"), SW_DATUM_INTEGER(GUCR_INTERLEAVINGSTYLE_PIXEL),
    SW_DATUM_STRING("PackingUnitRequest"),
      SW_DATUM_ARRAY(&hwa1_packing[0], SW_DATA_ARRAY_LENGTH(hwa1_packing)),
  /** \todo ajcd 2009-09-23: We can only use Private /ColorChannels as a
     selection key for this because pixel-interleaved currently cannot add
     new channels. Therefore, gucr_framesChannelsTotal(gucr_framesStart()) is
     always its original value of 3, as initialised from ColorChannels. */
    SW_DATUM_STRING("Private"),
      SW_DATUM_DICT(&hwa1_private_color[0], SW_DATA_DICT_LENGTH(hwa1_private_color)),
    SW_DATUM_STRING("ProcessColorModel"), SW_DATUM_STRING("DeviceRGB"),
    SW_DATUM_STRING("RunLength"), SW_DATUM_BOOLEAN(FALSE),
    SW_DATUM_STRING("ValuesPerComponent"), SW_DATUM_INTEGER(256),
  } ;

  /* Alternative page device dicts for hwa1 surface. */
  const static sw_datum hwa1_pagedevs[] = {
    SW_DATUM_DICT(&hwa1_dict_mono[0], SW_DATA_DICT_LENGTH(hwa1_dict_mono)),
    SW_DATUM_DICT(&hwa1_dict_mono2[0], SW_DATA_DICT_LENGTH(hwa1_dict_mono2)),
    SW_DATUM_DICT(&hwa1_dict_color[0], SW_DATA_DICT_LENGTH(hwa1_dict_color)),
  } ;

  static surface_set_t hwa1_set =
    SURFACE_SET_INIT(SW_DATUM_ARRAY(&hwa1_pagedevs[0],
                                    SW_DATA_ARRAY_LENGTH(hwa1_pagedevs))) ;

  static const surface_t *indexed[N_SURFACE_TYPES] ;

  /* The surface */
  static surface_t hwa1 = SURFACE_INIT ;

  /* No HWA API initially. */
  hwa_api = 0 ;

  /* Init our tables */
  init_ROP2() ;

  /* Base blits required, all others aren't */
  hwa1.baseblits[BLT_CLP_NONE].spanfn =
    hwa1.baseblits[BLT_CLP_RECT].spanfn =
  hwa1.baseblits[BLT_CLP_COMPLEX].spanfn = hwa1_span ;

  hwa1.baseblits[BLT_CLP_NONE].blockfn =
    hwa1.baseblits[BLT_CLP_RECT].blockfn =
  hwa1.baseblits[BLT_CLP_COMPLEX].blockfn = hwa1_block ;

  hwa1.baseblits[BLT_CLP_NONE].charfn =
    hwa1.baseblits[BLT_CLP_RECT].charfn =
  hwa1.baseblits[BLT_CLP_COMPLEX].charfn = hwa1_char ;

  hwa1.baseblits[BLT_CLP_NONE].imagefn =
    hwa1.baseblits[BLT_CLP_RECT].imagefn =
    hwa1.baseblits[BLT_CLP_COMPLEX].imagefn = hwa1_image ;

  hwa1.areafill = hwa1_erase ;

  /* HWA1 only supports black and white brushes (PCL patterns). We can use
     the PCL pattern layer to replicate and set the color for any other
     patterns that occur. */
  init_pcl_pattern_blit(&hwa1) ;

  /* Built-ins */
  surface_intersect_builtin(&hwa1) ;
  surface_pattern_builtin(&hwa1) ;
  surface_gouraud_builtin_tone_multi(&hwa1) ;

  hwa1.prepare                = hwa1_render_prepare ;
  hwa1.n_rollover             = 3 ;
  hwa1.screened               = FALSE ;
  hwa1.image_depth            = 8 ;
  hwa1.render_order           = SURFACE_ORDER_DEVICELR|SURFACE_ORDER_DEVICETB ;

  /* Install the builtin span clip surface, swap it out for the HWA clip
     surface, saving the original. */
  builtin_clip_N_surface(&hwa1, NULL) ;
  hwa1_clip_surface = *hwa1.clip_surface ; /* Copy surface implementation */
  span_complex_clip = hwa1_clip_surface.complex_clip ;
  hwa1_clip_surface.complex_clip = hwa1_complex_clip ;
  hwa1_clip_surface.rect_clip = hwa1_rect_clip ;
  hwa1.clip_surface = &hwa1_clip_surface ;

  /* The surface we've just completed is part of a set. */
  indexed[SURFACE_OUTPUT]    = &hwa1 ;


  hwa1_set.indexed           = indexed ;
  hwa1_set.n_indexed         = NUM_ARRAY_ITEMS(indexed) ;
  hwa1_set.rop_support       = hwa1_rop_support ;
  hwa1_set.sheet_begin       = hwa1_sheet_begin ;
  hwa1_set.sheet_end         = hwa1_sheet_end ;
  hwa1_set.band_localiser    = hwa1_band_localise ;
  hwa1_set.select            = hwa1_select ;
  hwa1_set.deselect          = hwa1_deselect ;

  /* hwa1_set.prefers_bitmaps   = TRUE ; */

  /* Built-ins */
  surface_set_trap_builtin(&hwa1_set, indexed);
  surface_set_transparency_builtin(&hwa1_set, &hwa1, indexed) ;

  /* And register it */
  surface_set_register(&hwa1_set) ;

# ifdef HWA1_METRICS
  hwa1_metrics_reset(0) ;
# endif
}

/* Log stripped */
