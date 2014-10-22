/** \file
 * \ingroup pcl5
 *
 * $HopeName: COREpcl_pcl5!src:resourcecache.h(EBDSDK_P.1) $
 *
 * Copyright (C) 2008-2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 *
 * \brief
 * Resource cache methods restricted to this compound. pcl5resources.h defines
 * methods and types with wider visibility.
 */
#ifndef _resourcecache_h_
#define _resourcecache_h_

#include "pcl5resources.h"
#include "pcl5context.h"

/* PCL5 resource types. */
enum {
  SW_PCL5_FONT = 0,
  SW_PCL5_MACRO,
  SW_PCL5_PATTERN,
  SW_PCL5_SYMBOL_SET,
  SW_PCL5_FONT_EXTENDED,
  SW_PCL5_ALL
} ;

/* Opaque type definition. */
typedef struct PCL5IdCacheEntry PCL5IdCacheEntry;

/**
 * Iterator structure; this should be considered private.
 */
typedef struct {
  PCL5IdCache* cache;
  int32 index;
  PCL5IdCacheEntry* entry;
} PCL5IdCacheIterator;

#define MAX_STRING_ID_LENGTH  (253)

/**
 * Free the pcl5_resource_string_id buffer and set length to zero.
 * N.B. This is safe to use with a Null buffer.
 */
void pcl5_cleanup_ID_string(pcl5_resource_string_id *string_id);

/**
 * Copy the pcl5_resource_string_id.
 * N.B. This expects an empty to_string buffer, (or the same
 *      one as the from_string buffer).
 */
Bool pcl5_copy_ID_string(pcl5_resource_string_id *to_string,
                         pcl5_resource_string_id *from_string) ;

/**
 * Start an iteration on the specified cache.
 */
void pcl5_id_cache_start_interation(PCL5IdCache* id_cache,
                                    PCL5IdCacheIterator* iterator);

/**
 * Return the next cache entry in the iteration. Returns NULL when the iteration
 * has finished.
 */
pcl5_resource* pcl5_id_cache_iterate(PCL5IdCacheIterator* iterator);

/**
 * Release the pattern raster data for the specified entry.
 */
void pcl5_id_cache_release_pattern_data(PCL5IdCache *id_cache, pcl5_resource_numeric_id id);

/**
 * Clear all aliased fonts.
 */
void reset_aliased_fonts(PCL5_RIP_LifeTime_Context *pcl5_rip_context) ;

#endif

/* Log stripped */

