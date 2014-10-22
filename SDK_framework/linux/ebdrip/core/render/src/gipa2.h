/** \file
 * \ingroup bitblit
 *
 * $HopeName: CORErender!src:gipa2.h(trunk.0) $
 *
 * Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 *
 * \brief
 * RIFT project Graphics & Imaging Processing Accelerator interface.
 * This header file describes the Order List and Asset format for the GIPA2
 * device as described in version 1.66 of the specification.
 *
 * Written by the HWA1 Surface and consumed by the HWA API implementation.
 */

#ifndef __GIPA2_H__
#define __GIPA2_H__

#include "hqtypes.h"

/** \ingroup rendering */
/** \{ */

/* -------------------------------------------------------------------------- */
/* Command opcodes
 *
 * Documentation lists 'required' calls for drawing Polygons, Stretchblits and
 * Bitblits - these are indicated below but are unlikely to be complete.
 */

enum {
  GIPA2_COM_HEAD         = 0x01,

  GIPA2_PAGE_INIT        = 0x10,
  GIPA2_BAND_INIT,               /* P S B */

  GIPA2_DRAW_RUN         = 0x22, /* P     */
  GIPA2_DRAW_STRETCHBLT,         /*   S   */
  GIPA2_DRAW_BITBLT,             /*     B */
  GIPA2_DRAW_RUN_REPEAT,         /* P     */

  GIPA2_SET_ROP2         = 0x31, /* P   B */
  GIPA2_RESET_CLIP,
  GIPA2_SET_COLOR,               /* P     - +SET_DITHER_SIZE */
  GIPA2_SET_DITHER_STRETCHBLT,   /*   S   */
  GIPA2_SET_GAMMA_STRETCHBLT,    /*   S   */
  GIPA2_SET_DITHER_SIZE,         /* P     - before SET_COLOR */
  GIPA2_SET_CLIP,                /* P S B */
  GIPA2_SET_INDEXCOLOR,          /*   s   */
  GIPA2_SET_PENETRATIONCOLOR,    /*   S   */
  GIPA2_SET_USERGAMMA_STRETCHBLT,
  GIPA2_SET_GAMMA_STRETCHBLT2,

  GIPA2_SET_BRUSH = 0x41,        /* P     */

  GIPA2_BAND_DMA = 0x51,

  GIPA2_COM_BLOCK_END     = 0xFF,
  N_GIPA2_OPCODES
} ;

/* -------------------------------------------------------------------------- */
/* Command structures
 *
 * Some of these are available in two versions, for 16bit and 32bit coords.
 */

/* Macro to ensure we don't have platform issues */

#define CHECK_LENGTH(s, l) \
  typedef char s ## _is_wrong_length[(sizeof(s) == (l)) ? 1 : -1]

/* A 32bit pointer
 *
 * All pointers are absolute memory addresses. This interface is therefore
 * limited to 32bit platforms.
 *
 * Output band buffers can be outside the command buffer, but all other
 * structures must be inside.
 */

typedef uint8 * ptr32 ;

CHECK_LENGTH(ptr32, 4) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_COM_HEAD - start of a run of commands
 *
 * This structure must be 16byte aligned.
 */

typedef struct gipa2_com_head {
  uint8  opcode ;   /* GIPA2_COM_HEAD */
  uint8  nop1 ;     /* version not used */
  uint8  nop2 ;     /* revision not used */
  uint8  nop3 ;
  struct gipa2_com_head * next ; /* next gipa_com_head (16 byte aligned), or 0 for end */
  uint32 nop4 ;     /* command block size not used in GIPA2 */
  uint32 nop5 ;     /* data block size not used in GIPA2 */
} gipa2_com_head ;

CHECK_LENGTH(gipa2_com_head, 16) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_COM_BLOCK_END - end of a run of commands */

typedef struct {
  uint8 opcode ;    /* GIPA2_COM_BLOCK_END */
  uint8 nop1 ;
  uint8 nop2 ;
  uint8 nop3 ;
} gipa2_com_block_end ;

CHECK_LENGTH(gipa2_com_block_end, 4) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_BAND_DMA - end of a band */

typedef struct {
  uint8 opcode ;    /* GIPA2_BAND_DMA */
  uint8 nop1 ;
  uint8 nop2 ;
  uint8 nop3 ;
} gipa2_band_dma ;

CHECK_LENGTH(gipa2_band_dma, 4) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_PAGE_INIT - Page specification
 *
 * None of the fields are used by GIPA2, so this is a NOP and can be omitted.
 */

typedef struct {
  uint8  opcode ;   /* GIPA2_PAGE_INIT */
  uint8  nop1 ;     /* bit depth not used in GIPA2 */
  uint8  nop2 ;
  uint8  nop3 ;
  uint32 nop4 ;     /* page width unused on GIPA2 */
  uint32 nop5 ;     /* page height unused on GIPA2 */
} gipa2_page_init ;

CHECK_LENGTH(gipa2_page_init, 12) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_BAND_INIT - Band specification
 *
 * After this command, polygon clip is reset and rectangular clip set to the
 * whole band.
 */

typedef struct {
  uint8  opcode ;     /* GIPA2_BAND_INIT */
  uint8  planes ;     /* one bit per colorant, all 0 not allowed */
  uint8  bitdepth ;   /* see below */
  uint8  nop1 ;       /* was 'first page' flag */
  uint32 pixels ;     /* Width in pixels in the range: 256..32767 */
  uint32 bytes ;      /* Width in bytes rounded up to multiple of 8: 32..8192 */
  uint32 rows ;       /* Height in rows: 1..32767 */
  uint32 nop2 ;       /* Page width not used */
  uint32 nop3 ;       /* Page height not used */
  uint32 offset ;     /* Band Y offset: 0..65535 */
  ptr32  kband ;      /* Pointer to black band buffer - 16 byte aligned */
  ptr32  cband ;
  ptr32  mband ;
  ptr32  yband ;
} gipa2_band_init ;

CHECK_LENGTH(gipa2_band_init, 44) ;

/* Colorant names for 'planes' fields: */

enum {
  GIPA2_K = 1,         /* These can be orred together. All 0 is not allowed */
  GIPA2_C = 2,
  GIPA2_M = 4,
  GIPA2_Y = 8,
  GIPA2_CMY = 14,
  GIPA2_CMYK = 15      /* All four colorants */
} ;

/* Bit depths for the gipa2_band_init structure: */

enum {
  GIPA2_1BIT = 0,
  GIPA2_2BIT,
  GIPA2_4BIT
  /* 8bit not supported by GIPA2 */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_DRAW_RUN_REPEAT - rectangles
 *
 * There are two versions of this command, for 2 and 4 byte coordinates.
 */

typedef struct {
  uint8 opcode ;    /* GIPA2_DRAW_RUN_REPEAT */
  uint8 coords ;    /* GIPA2_COORDS_ARE_4 - see below */
  uint8 nop1 ;
  uint8 planes ;    /* one bit per colorant - GIPA_CMYK etc */
  int32 y ;         /* start row in range -65535..65535 */
  int32 x0 ;        /* start pixel in range -2^20..2^20 */
  int32 x1 ;        /* end pixel in range -2^20..2^20 */
  int32 rows ;      /* number of repeats in range 1..65535 */
} gipa2_draw_run_repeat_4 ;

CHECK_LENGTH(gipa2_draw_run_repeat_4, 20) ;

typedef struct {
  uint8  opcode ;   /* GIPA2_DRAW_RUN_REPEAT */
  uint8  coords ;   /* GIPA2_COORDS_ARE_2 - see below */
  uint8  nop1 ;
  uint8  planes ;   /* one bit per colorant - GIPA_CMYK etc */
  int16  y ;        /* start row in range -32768..32767 */
  int16  x0 ;       /* start pixel in range -32768..32767 */
  int16  x1 ;       /* end pixel in range -32768..32767 */
  uint16 rows ;     /* number of repeats in range 1..65535 */
} gipa2_draw_run_repeat_2 ;

CHECK_LENGTH(gipa2_draw_run_repeat_2, 12) ;

/* Coordinate sizes for 'coords' fields */
enum {
  GIPA2_COORDS_ARE_2 = 0,
  GIPA2_COORDS_ARE_4
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_DRAW_RUN - polygons
 *
 * There are two versions of this command, with and without Y clipping.
 */

typedef struct {
  uint8  opcode ;     /* GIPA2_DRAW_RUN */
  uint8  coords ;     /* GIPA2_COORDS_ARE_2 or GIPA2_COORDS_ARE_4 */
  uint8  flags ;      /* yclip and compression - see below */
  uint8  planes ;     /* GIPA2_CMYK etc */
  uint32 nop1 ;       /* data size not used */
  ptr32  scanlines ;  /* ptr to bytes for DLDE, int16s or int32s otherwise */
} gipa2_draw_run ;

CHECK_LENGTH(gipa2_draw_run, 12) ;

typedef struct {
  uint8  opcode ;     /* GIPA2_DRAW_RUN */
  uint8  coords ;     /* COORDS_ARE_2 or COORDS_ARE_4 */
  uint8  flags ;      /* yclip and compression - see below */
  uint8  planes ;     /* GIPA2_CMYK etc */
  uint32 nop1 ;       /* data size not used */
  ptr32  scanlines ;  /* ptr to bytes for DLDE, int16s or int32s otherwise */
  int32  y0 ;         /* minimum Y coordinate in range -65535..65535 */
  int32  y1 ;         /* maximum Y coordinate in range y0..65535 */
} gipa2_draw_run_yclip ;

CHECK_LENGTH(gipa2_draw_run_yclip, 20) ;

/* Flags for gipa2_draw_run */
enum {
  GIPA2_NOCOMP = 0,    /* No compression - scanlines are int16s or int32s */
  GIPA2_DLDE = 1,      /* Compression - scanlines are uint8s */
  GIPA2_YCLIP = 128,   /* structure is gipa_draw_run_yclip */
  GIPA2_NOCLIP = 0     /* structure is gipa_draw_run */
} ;

/* Scanline data with 2 byte coords:
 * int16 c[]          must be 8byte aligned
 *       c[n+0]       y coordinate in range 0..32767 or -1 for end
 *       c[n+1]       start x in range -32768..32767
 *       c[n+2]       end x in range x0..32767
 * n+=3  ...
 */

/* Scanline data with 4 byte coords:
 * int32 c[]          must be 8byte aligned
 *       c[n+0]       y coordinate in range 0..2147483647 or -1 for end
 *       c[n+1]       start x in range -2147483648..2147483647
 *       c[n+2]       end x in range x0..2147483647
 * n+=3  ...
 */

/* Scanline data with DLDE compression:
 * uint32 type_size   b16..31 Type: b19 = 12bit packing of Start Run Information
 *                                  b20 = 14bit packing
 *                                  b24 = 16bit packing
 *                            Only one of the above can be set, if all are 0,
 *                            packing is 32bit - see GIPA2_DLDE types below.
 *                    b0..15  Size: The number of bytes following this word,
 *                                  rounded up to a multiple of 4 bytes.
 *
 * uint32 startrun[]  One to three words of packed Start Run Information. This
 *                    is the first span which will be delta modified by the
 *                    byte-encoded "Run Data Columns" following.
 *                    12bit: [0] b20..31 StartY 0..4095
 *                               b8..19  StartX 0..4095
 *                               b0..7   DeltaX 0..255 (EndX = StartX + DeltaX)
 *                    14bit: [0] b18..31 StartY 0..16383
 *                               b4..17  StartX 0..16383
 *                               b0..3   DeltaX 0..15
 *                    16bit: [0] b16..31 StartY -32768..32767
 *                               b0..15  StartX -32768..32767
 *                           [1] b16..31 nop    0
 *                               b0..15  DeltaX 0..65535
 *                    32bit: [0] StartY (signed)
 *                           [1] StartX (signed)
 *                           [2] EndX (not DeltaX)
 *
 * uint8 rundata[]    Multiple bytes of delta modification to the previous span.
 *                    There are many record formats, indicated by the leading
 *                    bits of the first byte of the record, with multiple
 *                    records until the End marker:
 *                    [0] b5..7 Record type: 0 = End marker
 *                                           1 = Micro run (2bit)
 *                                           2 = Uniform run (4bit)
 *                                           3 = Short run (4bit)
 *                                           4 = Long run (8+8bit)
 *                                           5 = Long run (8+4bit)
 *                                           6 = Long run (16+16bit | 32+32bit)
 *                                           7 = Long run (16+4bit)
 *                    Type 0: End marker
 *                    [0] b4    Must be 0
 *                        b0..3 Anything
 *                    [n] Claimed to be padding to 4-byte alignment, but assets
 *                        must be 8-byte aligned in general.
 *
 *                    Type 1: Micro run - DeltaY is +1 for each span
 *                    [0] b0..4 Number of spans (delta pairs) 1..31
 *                    [n] b6..7 DeltaStartX+1 0..3 = -1..+2
 *                        b4..5 DeltaEndX+1   0..3 = -1..+2
 *                        b2..3 next DeltaStartX if appropriate
 *                        b0..1 next DeltaEndX if appropriate
 *
 *                    Type 2: Uniform run - short and long count versions
 *                    [0] b4    DeltaY 0..+1
 *                        b0..3 Repeat count 2..15, or 0 for long count
 *                    [1] b0..7 Long repeat count if above is 0, else omitted
 *                    [1|2] b4..7 DeltaStartX+7 0..15 = -7..+8 } Incremental for
 *                          b0..3 DeltaEndX+7   0..15 = -7..+8 } every repeat
 *
 *                    Type 3: Short run - DeltaY is +1 for each span
 *                    [0] b0..4 Number of spans (delta pairs) 1..31
 *                    [n] b4..7 DeltaStartX+7 0..3 = -7..+8
 *                        b0..3 DeltaEndX+7   0..3 = -7..+8
 *
 *                    Type 4: Long run 8+8bit
 *                    [0] b4    DeltaY 0..+1
 *                        b0..3 Must be 0
 *                    [1] b0..7 DeltaStartX -128..+127 (signed)
 *                    [2] b0..7 DeltaEndX   -128..+127 (signed)
 *
 *                    Type 5: Long run 8+4bit
 *                    [0] b4    DeltaY 0..+1
 *                        b0..3 Delta2EndX+7 0..15 = -7..+8
 *                    [1] DeltaStartX -128..+127 (signed)
 *                    DeltaEndX = DeltaStartX + Delta2EndX
 *
 *                    Type 6: Long run 16+16bit or 32+32bit
 *                    [0] b4    DeltaY 0..+1
 *                        b0..3 Delta size: 0 = Shorts, 15 = Longs
 *                    [1..2] or [1..4] DeltaStartX (signed)
 *                    [3..4] or [5..8] DeltaEndX   (signed)
 *
 *                    Type7: Long run 16+4bit
 *                    [0] b4    DeltaY 0..+1
 *                        b0..3 Delta2EndX+7 0..15 = -7..+8
 *                    [1..2]    DeltaStartX (signed)
 *                    DeltaEndX = DeltaStartX + Delta2EndX
 */

/* DLDE types to be orred with the data size */
enum {
  GIPA2_DLDE_12BIT = 1 << 19, /* Start run is 12+12+8bit */
  GIPA2_DLDE_14BIT = 1 << 20, /* Start run is 14+14+4bit */
  GIPA2_DLDE_16BIT = 1 << 24, /* Start run is 16+16+16bit */
  GIPA2_DLDE_32BIT = 0        /* Start run is 32+32+32bit */
};
/* -------------------------------------------------------------------------- */
/* GIPA2_DRAW_BITBLT - rectangular image copy with ROP
 *
 * Note that this can only do word-aligned copies, which isn't very useful.
 */

typedef struct {
  uint8  opcode ;     /* GIPA2_DRAW_BITBLT */
  uint8  nop1 ;
  uint8  nop2 ;
  uint8  planes ;     /* GIPA2_CMYK etc (anded with band planes) */
  int32  width ;      /* 1bit: 32..16384, 2bit: 16..16320, 4bit: 8..8160 */
  int32  height ;     /* rows: 1..16384 */
  int32  x ;          /* -16383..16383 in multiples of 1bit:32 2bit:16 4bit:8 */
  int32  y ;          /* -65535..65535 */
  int32  nop3 ;       /* data size not used */
  ptr32  data ;       /* data pointer must be 8byte aligned */
} gipa2_draw_bitblt ;

CHECK_LENGTH(gipa2_draw_bitblt, 28) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_DRAW_STRETCHBLT - general purpose image */

typedef struct {
  uint8  opcode ;     /* GIPA2_DRAW_STRETCHBLT */
  uint8  nop1 ;
  uint8  flags ;      /* See below */
  uint8  planes ;     /* GIPA2_CMYK etc (anded with band planes) */
  int32  srcwidth ;   /* source image width in pixels: 1..10239 */
  int32  srcheight ;  /* source image height in rows: 1..16384 */
  int32  width ;      /* destination width: 1..16352 (4bit:8192) */
  int32  height ;     /* destination height: 1..16384 */
  int32  nop2 ;
  int32  xscale ;     /* 65536 * horizontal scaling */
  int32  yscale ;     /* 65536 * vertical scaling */
  int32  x ;          /* left edge in pixels: -16383..16383 */
  int32  y ;          /* top edge in rows: -65535..65535 */
  uint8  kxoffset ;   /* black dither pattern x offset: 0..ditherheight-1 */
  uint8  cxoffset ;   /* cyan... */
  uint8  mxoffset ;   /* magenta... */
  uint8  yxoffset ;   /* yellow... */
  uint8  kyoffset ;   /* black dither pattern y offset 0..ditherwidth-1 */
  uint8  cyoffset ;   /* cyan... */
  uint8  myoffset ;   /* magenta... */
  uint8  yyoffset ;   /* yellow... */
  uint32 nop3 ;       /* data size not used */
  ptr32  data ;       /* 8byte aligned */
} gipa2_draw_stretchblt ;

CHECK_LENGTH(gipa2_draw_stretchblt, 56) ;

/* STRETCHBLT flags */
enum {
  /* Image depth - always use one of these: */
  GIPA2_IMAGE_PLANES = 0x00,  /* Separate 8bit C,M,Y,K planes */
  GIPA2_IMAGE_RGB = 0x21,     /* 24bit */
  GIPA2_IMAGE_CMY = 0x01,     /* 24bit */
  GIPA2_IMAGE_CMYK = 0x02,    /* Interleaved CMYK */
  /* Other flags - orred with the above: */
  GIPA2_IMAGE_INDEXED = 0x10, /* paletted image (not allowed for CMY/RGB) */
  GIPA2_IMAGE_BGR = 0x40,     /* reverse normal ordering */
  GIPA2_IMAGE_TRANS = 0x80    /* PENETRATION_COLOR used (after ROP) */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_ROP2 - change logical operation */

typedef struct {
  uint8  opcode ;     /* GIPA2_SET_ROP2 */
  uint8  rop2 ;       /* From the ROPxx or ROP2_* enumerations below */
  uint8  nop1 ;
  uint8  nop2 ;
} gipa2_set_rop2 ;

CHECK_LENGTH(gipa2_set_rop2, 4) ;

/* GIPA ROP symbols - official but not very informative: */
enum {
  ROP00 = 0,
  ROP11 = 0x11,
  ROP22 = 0x22,
  ROP33 = 0x33,
  ROP44 = 0x44,
  ROP55 = 0x55,
  ROP66 = 0x66,
  ROP77 = 0x77,
  ROP88 = 0x88,
  ROP99 = 0x99,
  ROPAA = 0xAA,
  ROPBB = 0xBB,
  ROPCC = 0xCC,
  ROPDD = 0xDD,
  ROPEE = 0xEE,
  ROPFF = 0xFF
} ;

/* Functional ROP3-style symbols and their ROP3 equivalents for S and T: */
enum {
  ROP2_0    = 0,
  ROP2_Blk  = 0,    /* ROP3   0 */
  ROP2_DSan = 0x11, /* ROP3 119 */
  ROP2_DTan = 0x11, /* ROP3  95 */
  ROP2_DSno = 0x22, /* ROP3 187 */
  ROP2_DTno = 0x22, /* ROP3 175 */
  ROP2_Sn   = 0x33, /* ROP3  51 */
  ROP2_Tn   = 0x33, /* ROP3  15 */
  ROP2_SDno = 0x44, /* ROP3 221 */
  ROP2_TDno = 0x44, /* ROP3 245 */
  ROP2_Dn   = 0x55, /* ROP3  85 */
  ROP2_DSxn = 0x66, /* ROP3 153 */
  ROP2_TDxn = 0x66, /* ROP3 165 - note: not DTxn for some reason */
  ROP2_DSon = 0x77, /* ROP3  17 */
  ROP2_DTon = 0x77, /* ROP3   5 */
  ROP2_DSo  = 0x88, /* ROP3 238 */
  ROP2_DTo  = 0x88, /* ROP3 250 */
  ROP2_DSx  = 0x99, /* ROP3 102 */
  ROP2_DTx  = 0x99, /* ROP3  90 */
  ROP2_D    = 0xAA, /* ROP3 170 */
  ROP2_DSna = 0xBB, /* ROP3  34 */
  ROP2_DTna = 0xBB, /* ROP3  10 */
  ROP2_S    = 0xCC, /* ROP3 204 */
  ROP2_T    = 0xCC, /* ROP3 240 */
  ROP2_SDna = 0xDD, /* ROP3  68 */
  ROP2_TDna = 0xDD, /* ROP3  80 */
  ROP2_DSa  = 0xEE, /* ROP3 136 */
  ROP2_DTa  = 0xEE, /* ROP3 160 */
  ROP2_Wht  = 0xFF, /* ROP3 255 */
  ROP2_1    = 0xFF
} ;

/* See end of this file for the full ROP3 set */

/* -------------------------------------------------------------------------- */
/* GIPA2_RESET_CLIP - reset clip rectangle to band boundary */

typedef struct {
  uint8  opcode ;     /* GIPA2_RESET_CLIP */
  uint8  nop1 ;
  uint8  nop2 ;
  uint8  nop3 ;
} gipa2_reset_clip ;

CHECK_LENGTH(gipa2_reset_clip, 4) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_COLOR - specify dither for graphics
 *
 * There are two versions of this command - with and without background color.
 *
 * Dither cell dimensions are specified in the separate SET_DITHER_SIZE command.
 */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_COLOR */
  uint8 bg ;          /* GIPA2_COLOR_NOBG */
  uint8 nop1 ;
  uint8 nop2 ;
  ptr32 kfore ;       /* K Dither cell for foreground - 8 byte aligned */
  ptr32 cfore ;
  ptr32 mfore ;
  ptr32 yfore ;
} gipa2_set_color ;

CHECK_LENGTH(gipa2_set_color, 20) ;

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_COLOR */
  uint8 bg ;          /* GIPA2_COLOR_BG */
  uint8 nop1 ;
  uint8 nop2 ;
  ptr32 kfore ;       /* K Dither cell for foreground - 8 byte aligned */
  ptr32 cfore ;
  ptr32 mfore ;
  ptr32 yfore ;
  ptr32 kback ;       /* K Dither cell for background - 8 byte aligned */
  ptr32 cback ;
  ptr32 mback ;
  ptr32 yback ;
} gipa2_set_color_bg ;

CHECK_LENGTH(gipa2_set_color_bg, 36) ;

/* SET_COLOR bg flags */
enum {
  GIPA2_COLOR_NOBG = 0,  /* background color not specified */
  GIPA2_COLOR_BG = 1     /* backgroudn color specified */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_DITHER_STRETCHBLT - specify dither for stretchblt
 *
 * There are two versions of this command - with and without Y clipping.
 *
 * For 4bit output, sizes are reduced to: 4,8,12,16,20,24,28,32,40
 */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_DITHER_STRETCHBLT */
  uint8 flags ;       /* GIPA2_DITHER_NOCLIP+ see below */
  uint8 nop1 ;
  uint8 nop2 ;
  uint8 kwidth ;      /* K dither cell width: 16,20,24,32,40,48,56,64 */
  uint8 kheight ;     /* K dither cell height: 16,20,24,32,40,48,56,64 */
  int16 ksize ;       /* not used by GIPA2 */
  ptr32 kcell ;       /* K dither cell address - 8 byte aligned */
  uint8 cwidth ;      /* C dither cell width */
  uint8 cheight ;     /* C dither cell height */
  int16 csize ;       /* not used by GIPA2 */
  ptr32 ccell ;       /* C dither cell address - 8 byte aligned */
  uint8 mwidth ;      /* M dither cell width */
  uint8 mheight ;     /* M dither cell height */
  int16 msize ;       /* not used by GIPA2 */
  ptr32 mcell ;       /* M dither cell address - 8 byte aligned */
  uint8 ywidth ;      /* Y dither cell width */
  uint8 yheight ;     /* Y dither cell height */
  int16 ysize ;       /* not used by GIPA2 */
  ptr32 ycell ;       /* Y dither cell address - 8 byte aligned */
} gipa2_set_dither_stretchblt ;

CHECK_LENGTH(gipa2_set_dither_stretchblt, 36) ;

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_DITHER_STRETCHBLT */
  uint8 flags ;       /* GIPA2_DITHER_YCLIP+ see below */
  uint8 nop1 ;
  uint8 nop2 ;
  uint8 kwidth ;      /* K dither cell width: 16,20,24,32,40,48,56,64 */
  uint8 kheight ;     /* K dither cell height: 16,20,24,32,40,48,56,64 */
  int16 ksize ;       /* not used by GIPA2 */
  ptr32 kcell ;       /* K dither cell address - 8 byte aligned */
  uint8 cwidth ;      /* C dither cell width */
  uint8 cheight ;     /* C dither cell height */
  int16 csize ;       /* not used by GIPA2 */
  ptr32 ccell ;       /* C dither cell address - 8 byte aligned */
  uint8 mwidth ;      /* M dither cell width */
  uint8 mheight ;     /* M dither cell height */
  int16 msize ;       /* not used by GIPA2 */
  ptr32 mcell ;       /* M dither cell address - 8 byte aligned */
  uint8 ywidth ;      /* Y dither cell width */
  uint8 yheight ;     /* Y dither cell height */
  int16 ysize ;       /* not used by GIPA2 */
  ptr32 ycell ;       /* Y dither cell address - 8 byte aligned */
  int32 y0 ;          /* lowest Y coordinate -65535..65535*/
  int32 y1 ;          /* highest Y coordinate -65535..65535 */
} gipa2_set_dither_stretchblt_yclip ;

CHECK_LENGTH(gipa2_set_dither_stretchblt_yclip, 44) ;

/* SET_DITHER_STRETCHBLT flags */
enum {
  GIPA2_DITHER_GE = 0,    /* pixel >= threshold */
  GIPA2_DITHER_GT = 1,    /* pixel > threshold */

  GIPA2_DITHER_255 = 0,   /* 255 is 255 */
  GIPA2_DITHER_MINUS1 = 2,/* 255 is -1 */

  GIPA2_DITHER_NOCLIP = 0,/* no Y clip specified */
  GIPA2_DITHER_YCLIP = 4  /* Y clip specified */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_USERGAMMA_STRETCHBLT - specify user gamma curve for stretchblt */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_USERGAMMA_STRETCHBLT */
  uint8 flags ;       /* see below */
  uint16 nop ;
  ptr32 user ;        /* Pointer to user gamma data - 8 byte aligned */
} gipa2_set_usergamma_stretchblt ;

CHECK_LENGTH(gipa2_set_usergamma_stretchblt, 8) ;

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_USERGAMMA_STRETCHBLT */
  uint8 flags ;       /* see below - must have GIPA2_USERGAMMA_YCLIP set */
  uint16 nop ;
  ptr32 user ;        /* Pointer to user gamma data - 8 byte aligned */
  int32 y0 ;
  int32 y1 ;
} gipa2_set_usergamma_stretchblt_yclip ;

CHECK_LENGTH(gipa2_set_usergamma_stretchblt_yclip, 16) ;

/* SET_USERGAMMA flags */
enum {
  GIPA2_NOUSERGAMMA = 0,      /* Whether user gamma is performed */
  GIPA2_USERGAMMA = 1,

  GIPA2_USERGAMMA_NOCLIP = 0, /* No Y clip specified */
  GIPA2_USERGAMMA_YCLIP = 2   /* Y clip specified */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_GAMMA_STRETCHBLT - specify gamma curve for stretchblt */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_GAMMA_STRETCHBLT */
  uint8 flags ;       /* see below */
  int16 tonerlimit ;  /* Toner limit 255..1023 */
  ptr32 gamma ;       /* Address of gamma table - 8 byte aligned */
  ptr32 black ;       /* Address of black generation LUT - 8 byte aligned */
  ptr32 cmm ;         /* Address of CMM LUT - 8 byte aligned */
} gipa2_set_gamma_stretchblt ;

CHECK_LENGTH(gipa2_set_gamma_stretchblt, 16) ;

/* SET_GAMMA flags */
enum {
  GIPA2_NOBGUCG = 0,        /* Whether black is generated */
  GIPA2_BGUCR = 1,

  GIPA2_NOCMM = 0,          /* Whether colors are managed */
  GIPA2_CMM = 2,

  GIPA2_NOTONERLIMIT = 0,   /* Whether toner limiting happens */
  GIPA2_TONERLIMIT = 4,

  GIPA2_NOGAMMA = 0,        /* Whether gamma processing occurs */
  GIPA2_GAMMA = 8,

  GIPA2_NOTONERMASS = 0,    /* Whether toner mass adjustment is performed */
  GIPA2_TONERMASS = 16,

  GIPA2_NOPOSTCMMGRAY = 0,  /* Whether gray is determined after CMM */
  GIPA2_POSTCMMGRAY = 32,

  GIPA2_NOPRECMMGRAY = 0,   /* Whether gray is decided before CMM */
  GIPA2_PRECMMGRAY = 64,

  GIPA2_K2K = 0,            /* Whether black is preserved */
  GIPA2_G2K = 128
} ;

/* Gamma table:
 *   uint8 gammatable[256]
 *
 * Black generation LUT:
 *   uint8 blackLUT[...]
 *
 * CMM LUT:
 *   uint8 cmmLUT[...]
 */

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_GAMMA_STRETCHBLT2 - specify gamma curve for stretchblt with Y clip
 *
 * This is identical to the above GIPA_SET_GAMMA_STRETCHBLT but with an added
 * Y clipping range.
 */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_GAMMA_STRETCHBLT2 */
  uint8 flags ;       /* see above */
  int16 tonerlimit ;  /* Toner limit 255..1023 */
  ptr32 gamma ;       /* Address of gamma table - 8 byte aligned */
  ptr32 black ;       /* Address of black generation LUT - 8 byte aligned */
  ptr32 cmm ;         /* Address of CMM LUT - 8 byte aligned */
  int32 y0 ;          /* lowest Y coordinate -65535..65535 */
  int32 y1 ;          /* highest Y coordinate -65535..65535 */
} gipa2_set_gamma_stretchblt2 ;

CHECK_LENGTH(gipa2_set_gamma_stretchblt2, 24) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_DITHER_SIZE - specify dither dimensions for graphics
 *
 * For 4bit output, sizes are reduced to: 4,8,12,16,20,24,28,43,40,48
 *
 * Dither cell addresses are specified in the seperate SET_COLOR command.
 */

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_DITHER_SIZE */
  uint8 nop1 ;
  uint8 nop2 ;
  uint8 nop3 ;
  uint8 kwidth ;      /* K dither cell width: 16,20,24,32,40,48,56,64 */
  uint8 kheight ;     /* K dither cell height: 16,20,24,32,40,48,56,64 */
  uint8 cwidth ;      /* C dither cell width */
  uint8 cheight ;     /* C dither cell height */
  uint8 mwidth ;      /* M dither cell width */
  uint8 mheight ;     /* M dither cell height */
  uint8 ywidth ;      /* Y dither cell width */
  uint8 yheight ;     /* Y dither cell height */
} gipa2_set_dither_size ;

CHECK_LENGTH(gipa2_set_dither_size, 12) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_CLIP - specify the rectangular or polygon clip
 *
 * There are two versions of this structure - for 2 and 4 byte coordinates.
 */

typedef struct {
  uint8  opcode ;     /* GIPA2_SET_CLIP */
  uint8  flags ;      /* Clip type (see below) orred with GIPA2_COORDS_ARE_2 */
  uint8  limit ;      /* Horizontal polygon clip limitation (see below) */
  uint8  nop1 ;
  uint16 x ;          /* left edge of clip: 0..32767 */
  uint16 y ;          /* top edge of clip: 0..65535 */
  uint16 width ;      /* clip width (exclusive) 1..32767 */
  uint16 height ;     /* clip height (exclusive) 1..65535 */
  uint32 nop2 ;       /* data size not used */
  ptr32  data ;       /* Pointer to scanlines or bitmap for non-rect cases
                       * 8byte aligned */
} gipa2_set_clip_2 ;

CHECK_LENGTH(gipa2_set_clip_2, 20) ;

typedef struct {
  uint8 opcode ;      /* GIPA2_SET_CLIP */
  uint8 flags ;       /* Clip type (see below) orred with GIPA2_COORDS_ARE_4 */
  uint8 limit ;       /* Horizontal polygon clip format (see below) */
  uint8 nop1 ;
  int32 x ;           /* left edge of clip: 0..32767 */
  int32 y ;           /* top edge of clip: 0..65535 */
  int32 width ;       /* clip width (exclusive) 1..32767 */
  int32 height ;      /* clip height (exclusive) 1..65535 */
  int32 nop2 ;        /* data size not used */
  ptr32 data ;        /* Pointer to scanlines or bitmap for non-rect cases
                       * 8byte aligned */
} gipa2_set_clip_4 ;

CHECK_LENGTH(gipa2_set_clip_4, 28) ;

/* Clip type, orred with COORDS_ARE_2 or COORDS_ARE_4 */
enum {
  GIPA2_CLIP_RECT = 0x00, /* data ptr not used */
  GIPA2_CLIP_RUN  = 0x10, /* in one of a number of formats */
  GIPA2_CLIP_BITMAP = 0x20 /* only valid during STRETCHBLT */
} ;

/* Horizontal polygon clip format.
 *
 * Each span is stored as a 16bit x_start, x_end pair with no y coordinate -
 * there is a span (or a set of spans) for every row of 'height'. Empty spans
 * are marked by making x0 > x1. In the case of single spans, GIPA_CLIP_RUN_1,
 * the spans are rounded up to 8byte boundaries anyway, so one might as well
 * use GIPA_CLIP_RUN_2 with the second spans of each row set to x0 > x1.
 *
 * The scanline data is then an array of these 16bit pairs (with nops for
 * GIPA_CLIP_RUN_1) in increasing Y order.
 */

enum {
  GIPA2_CLIP_RUN_1 = 0,  /* One 16bit x0,x1 followed by two 16bit nops */
  GIPA2_CLIP_RUN_2,      /* Two 16bit x0,x1 per scanline */
  GIPA2_CLIP_RUN_4,      /* Four 16bit x0,x1 per scanline */
  GIPA2_CLIP_RUN_8,      /* Eight 16bit x0,x1 per scanline */
  GIPA2_CLIP_RUN_16,     /* Sixteen 16bit x0,x1 per scanline */
} ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_INDEXCOLOR - specify palette to use for indexed STRETCHBLT.
 *
 * Color components depend on image color mode - CMYK, CMYx, RGBx or Gxxx. */

typedef struct {
  uint8  opcode ;       /* GIPA2_SET_INDEXED_COLOR */
  uint8  nop1 ;
  uint8  nop2 ;
  uint8  nop3 ;
  uint8  c0[4] ;        /* Color for index 0 */
  uint8  c1[4] ;        /* Color for index 1 */
} gipa2_set_indexcolor ;

CHECK_LENGTH(gipa2_set_indexcolor, 12) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_PENETRATION_COLOR - set the color to be treated as transparent
 *
 * Color components depend on image color mode - CMYK, CMYx, RGBx or Gxxx. */

typedef struct {
  uint8  opcode ;       /* GIPA2_SET_PENETRATION_COLOR */
  uint8  nop1 ;
  uint8  nop2 ;
  uint8  nop3 ;
  uint8  c[4] ;         /* Color to be invisible (after ROP) */
} gipa2_set_penetration_color ;

CHECK_LENGTH(gipa2_set_penetration_color, 8) ;

/* -------------------------------------------------------------------------- */
/* GIPA2_SET_BRUSH - set pattern to use for drawing */

typedef struct {
  uint8  opcode ;       /* GIPA2_SET_BRUSH */
  uint8  flags ;        /* opaque or transparent (see below) */
  uint8  nop1 ;
  uint8  nop2 ;
  uint32 nop3 ;         /* data size not used */
  uint8  width ;        /* 1..64 */
  uint8  height ;       /* 1..64 */
  uint8  xoffset ;      /* offset into pattern: 0..width-1 */
  uint8  yoffset ;      /* offset into pattern: 0..height-1 */
  ptr32  data ;         /* repeated to fill 64bits per row, padded to 64 rows */
} gipa2_set_brush ;

CHECK_LENGTH(gipa2_set_brush, 16) ;

/* Brush mode */
enum {
  GIPA2_BRUSH_OPAQUE = 0,
  GIPA2_BRUSH_TRANS
} ;

/* -------------------------------------------------------------------------- */

           /* Todo: document DLDE compressed scanline format */

/* ========================================================================== */
/* The ROP3 symbols for completeness: */

enum {
  ROP3_Blk = 0,
  ROP3_0 = 0,
  ROP3_DTSoon,
  ROP3_DTSona,
  ROP3_TSon,
  ROP3_SDTona,
  ROP3_DTon,
  ROP3_TDSxnon,
  ROP3_TDSaon,
  ROP3_SDTnaa,
  ROP3_TDSxon,
  ROP3_DTna,
  ROP3_TSDnaon,
  ROP3_STna,
  ROP3_TDSnaon,
  ROP3_TDSonon,
  ROP3_Tn,
  ROP3_TDSona,
  ROP3_DSon,
  ROP3_SDTxnon,
  ROP3_SDTaon,
  ROP3_DTSxnon,
  ROP3_DTSaon,
  ROP3_TSDTSanaxx,
  ROP3_SSTxDSxaxn,
  ROP3_STxTDxa,
  ROP3_SDTSanaxn,
  ROP3_TDSTaox,
  ROP3_SDTSxaxn,
  ROP3_TSDTaox,
  ROP3_DSTDxaxn,
  ROP3_TDSox,
  ROP3_TDSoan,
  ROP3_DTSnaa,
  ROP3_SDTxon,
  ROP3_DSna,
  ROP3_STDnaon,
  ROP3_STxDSxa,
  ROP3_TDSTanaxn,
  ROP3_SDTSaox,
  ROP3_SDTSxnox,
  ROP3_DTSxa,
  ROP3_TSDTSaoxxn,
  ROP3_DTSana,
  ROP3_SSTxTDxaxn,
  ROP3_STDSoax,
  ROP3_TSDnox,
  ROP3_TSDTxox,
  ROP3_TSDnoan,
  ROP3_TSna,
  ROP3_SDTnaon,
  ROP3_SDTSoox,
  ROP3_Sn,
  ROP3_STDSaox,
  ROP3_STDSxnox,
  ROP3_SDTox,
  ROP3_SDToan,
  ROP3_TSDToax,
  ROP3_STDnox,
  ROP3_STDSxox,
  ROP3_STDnoan,
  ROP3_TSx,
  ROP3_STDSonox,
  ROP3_STDSnaox,
  ROP3_TSan,
  ROP3_TSDnaa,
  ROP3_DTSxon,
  ROP3_SDxTDxa,
  ROP3_STDSanaxn,
  ROP3_SDna,
  ROP3_DTSnaon,
  ROP3_DSTDaox,
  ROP3_TSDTxaxn,
  ROP3_SDTxa,
  ROP3_TDSTDaoxxn,
  ROP3_DTSDoax,
  ROP3_TDSnox,
  ROP3_SDTana,
  ROP3_SSTxDSxoxn,
  ROP3_TDSTxox,
  ROP3_TDSnoan,
  ROP3_TDna,
  ROP3_DSTnaon,
  ROP3_DTSDaox,
  ROP3_STDSxaxn,
  ROP3_DTSonon,
  ROP3_Dn,
  ROP3_DTSox,
  ROP3_DTSoan,
  ROP3_TDSToax,
  ROP3_DTSnox,
  ROP3_DTx,
  ROP3_DTSDonox,
  ROP3_DTSDxox,
  ROP3_DTSnoan,
  ROP3_DTSDnaox,
  ROP3_DTan,
  ROP3_TDSxa,
  ROP3_DSTDSaoxxn,
  ROP3_DSTDoax,
  ROP3_SDTnox,
  ROP3_SDTSoax,
  ROP3_DSTnox,
  ROP3_DSx,
  ROP3_SDTSonox,
  ROP3_DSTDSonoxxn,
  ROP3_TDSxxn,
  ROP3_DTSax,
  ROP3_TSDTSoaxxn,
  ROP3_SDTax,
  ROP3_TDSTDoaxxn,
  ROP3_SDTSnoax,
  ROP3_TDSxnan,
  ROP3_TDSana,
  ROP3_SSDxTDxaxn,
  ROP3_SDTSxox,
  ROP3_SDTnoan,
  ROP3_DSTDxox,
  ROP3_DSTnoan,
  ROP3_SDTSnaox,
  ROP3_DSan,
  ROP3_TDSax,
  ROP3_DSTDSoaxxn,
  ROP3_DTSDnoax,
  ROP3_SDTxnan,
  ROP3_STDSnoax,
  ROP3_DTSxnan,
  ROP3_STxDSxo,
  ROP3_DTSaan,
  ROP3_DTSaa,
  ROP3_STxDSxon,
  ROP3_DTSxna,
  ROP3_STDSnoaxn,
  ROP3_SDTxna,
  ROP3_TDSTnoaxn,
  ROP3_DSTDSoaxx,
  ROP3_TDSaxn,
  ROP3_DSa,
  ROP3_SDTSnaoxn,
  ROP3_DSTnoa,
  ROP3_DSTDxoxn,
  ROP3_SDTnoa,
  ROP3_SDTSxoxn,
  ROP3_SSDxTDxax,
  ROP3_TDSanan,
  ROP3_TDSxna,
  ROP3_SDTSnoaxn,
  ROP3_DTSDToaxx,
  ROP3_STDaxn,
  ROP3_TSDTSoaxx,
  ROP3_DTSaxn,
  ROP3_DTSxx,
  ROP3_TSDTSonoxx,
  ROP3_SDTSonoxn,
  ROP3_DSxn,
  ROP3_DTSnax,
  ROP3_SDTSoaxn,
  ROP3_STDnax,
  ROP3_DSTDoaxn,
  ROP3_DSTDSaoxx,
  ROP3_TDSxan,
  ROP3_DTa,
  ROP3_TDSTnaoxn,
  ROP3_DTSnoa,
  ROP3_DTSDxoxn,
  ROP3_TDSTonoxn,
  ROP3_TDxn,
  ROP3_DSTnax,
  ROP3_TDSToaxn,
  ROP3_DTSoa,
  ROP3_DTSoxn,
  ROP3_D,
  ROP3_DTSono,
  ROP3_STDSxax,
  ROP3_DTSDaoxn,
  ROP3_DSTnao,
  ROP3_DTno,
  ROP3_TDSnoa,
  ROP3_TDSTxoxn,
  ROP3_SSTxDSxox,
  ROP3_SDTanan,
  ROP3_TSDnax,
  ROP3_DTSDoaxn,
  ROP3_DTSDTaoxx,
  ROP3_SDTxan,
  ROP3_TSDTxax,
  ROP3_DSTDaoxn,
  ROP3_DTSnao,
  ROP3_DSno,
  ROP3_STDSanax,
  ROP3_SDxTDxan,
  ROP3_DTSxo,
  ROP3_DTSano,
  ROP3_TSa,
  ROP3_STDSnaoxn,
  ROP3_STDSonoxn,
  ROP3_TSxn,
  ROP3_STDnoa,
  ROP3_STDSxoxn,
  ROP3_SDTnax,
  ROP3_TSDToaxn,
  ROP3_SDToa,
  ROP3_STDoxn,
  ROP3_DTSDxax,
  ROP3_STDSaoxn,
  ROP3_S,
  ROP3_SDTono,
  ROP3_SDTnao,
  ROP3_STno,
  ROP3_TSDnoa,
  ROP3_TSDTxoxn,
  ROP3_TDSnax,
  ROP3_STDSoaxn,
  ROP3_SSTxTDxax,
  ROP3_DTSanan,
  ROP3_TSDTSaoxx,
  ROP3_DTSxan,
  ROP3_TDSTxax,
  ROP3_SDTSaoxn,
  ROP3_DTSDanax,
  ROP3_STxDSxan,
  ROP3_STDnao,
  ROP3_SDno,
  ROP3_SDTxo,
  ROP3_SDTano,
  ROP3_TDSoa,
  ROP3_TDSoxn,
  ROP3_DSTDxax,
  ROP3_TSDTaoxn,
  ROP3_SDTSxax,
  ROP3_TDSTaoxn,
  ROP3_SDTSanax,
  ROP3_STxTDxan,
  ROP3_SSTxDSxax,
  ROP3_DSTDSanaxxn,
  ROP3_DTSao,
  ROP3_DTSxno,
  ROP3_SDTao,
  ROP3_SDTxno,
  ROP3_DSo,
  ROP3_SDTnoo,
  ROP3_T,
  ROP3_TDSono,
  ROP3_TDSnao,
  ROP3_TSno,
  ROP3_TSDnao,
  ROP3_TDno,
  ROP3_TDSxo,
  ROP3_TDSano,
  ROP3_TDSao,
  ROP3_TDSxno,
  ROP3_DTo,
  ROP3_DTSnoo,
  ROP3_TSo,
  ROP3_TSDnoo,
  ROP3_DTSoo,
  ROP3_1,
  ROP3_Wht = 0xFF
} ;

/** \} */

#endif /* __GIPA2_H__ */

/* -------------------------------------------------------------------------- */
/* Log stripped */
