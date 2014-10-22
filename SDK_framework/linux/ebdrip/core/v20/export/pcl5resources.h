/** \file
 * \ingroup interface
 *
 * $HopeName: SWcore_interface!pcl5:pcl5resources.h(EBDSDK_P.1) $
 *
 * Copyright (C) 2008-2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 *
 * This source code contains the confidential and trade secret information of
 * Global Graphics Software Ltd. It may not be used, copied or distributed for any
 * reason except as set forth in the applicable Global Graphics license agreement.
 *
 * \brief
 * Interface to gain access to PCL5 permanent macros and patterns outside
 * of the RIP.
 */

#ifndef __PCL5RESOURCES_H__
#define __PCL5RESOURCES_H__

/* Numeric ID's can be used for macros, fonts and patterns. */
typedef uint16 pcl5_resource_numeric_id ;

/* String ID's can be used for macros and fonts. */
typedef struct pcl5_resource_string_id {
  uint8 *buf ;
  int32 length ;
} pcl5_resource_string_id ;

/* ============================================================================
 * PCL5 resources
 * ============================================================================
 */

typedef struct pcl5_resource {
  /* One of the PCL5 resource types listed in the above
     enumeration. */
  int32 resource_type ;

  /* The numeric id of the resource, if one is used. */
  pcl5_resource_numeric_id numeric_id ;

  /* The string id of the resource, if one is used. */
  pcl5_resource_string_id string_id ;

  /* Is the resource permanent or not? */
  HqBool permanent ;

  /* Private data for the resource implementation. Can be NULL. */
  void *private_data ;
} pcl5_resource ;

typedef struct pcl5_pattern {
  pcl5_resource detail ; /* MUST be first member of structure. */

  int32 width, height ;
  int32 x_dpi, y_dpi ;

  /* True when the pattern uses the palette (and is not fixed
     black/white). */
  HqBool color ;

  /* The highest-numbered pen used in the pattern; only valid for HPGL
   * patterns. */
  int32 highest_pen;

  /* The number of bits per pixel; this is only valid for color
     patterns, and is either 1 or 8 bits. */
  int32 bits_per_pixel ;

  /* The number of bytes in each line. */
  int32 stride ;

  /* Binary pattern data. Each line is padded to the nearest byte. */
  uint8* data ;
} pcl5_pattern ;

typedef struct pcl5_macro {
  pcl5_resource detail ; /* MUST be first member of structure. */
  struct pcl5_macro *alias ;
} pcl5_macro ;



#endif  /* __PCL5RESOURCES_H__ */
