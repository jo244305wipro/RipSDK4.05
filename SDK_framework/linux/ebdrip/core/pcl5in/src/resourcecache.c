/** \file
 * \ingroup pcl5
 *
 * $HopeName: COREpcl_pcl5!src:resourcecache.c(EBDSDK_P.1) $
 * $Id: src:resourcecache.c,v 1.16.4.1.1.1 2013/12/19 11:25:01 anon Exp $
 *
 * Copyright (C) 2007-2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 *
 * \brief Implementation of our in RIP PCL5 resource cache. Holds
 * internal/downloaded macros and patterns.
 */

#include "core.h"
#include "resourcecache.h"
#include "fileio.h"
#include "hqmemcpy.h"
#include "hqmemcmp.h"

#include "pcl5context.h"
#include "pcl5context_private.h"
#include "factorypatterns.h"
#include "fontselection.h"
#include "macros.h"
#include "pagecontrol.h"

/* ============================================================================
 * String ID cache for macros
 * ============================================================================
 */

#define STRING_ID_CACHE_TABLE_SIZE 37

/* Because pcl5_resource is ALWAYS the first field of specfic resource
   type, we can simply use resource to check its type, id etc.. */
typedef struct PCL5StringIdCacheEntry {
  union {
    pcl5_resource detail ;
    pcl5_macro macro ;
    pcl5_font aliased_font ;
  } resource ;

  struct PCL5StringIdCacheEntry *next ;
} PCL5StringIdCacheEntry ;

struct PCL5StringIdCache {
  PCL5StringIdCacheEntry* table[STRING_ID_CACHE_TABLE_SIZE] ;
  uint32 table_size ;
} ;

void pcl5_cleanup_ID_string(pcl5_resource_string_id *string_id)
{
  HQASSERT(string_id != NULL, "string_id is NULL") ;

  if (string_id->buf != NULL) {
    mm_free(mm_pcl_pool, string_id->buf, string_id->length) ;
    string_id->buf = NULL ;
    string_id->length = 0 ;
  }
}

/* N.B. This expects an empty to_string buffer, (or the
 *      same one as the from_string buffer).
 */
Bool pcl5_copy_ID_string(pcl5_resource_string_id *to_string,
                         pcl5_resource_string_id *from_string)
{
  HQASSERT(to_string != NULL, "to_string is NULL") ;
  HQASSERT(from_string != NULL, "from_string is NULL") ;
  HQASSERT(to_string->buf == NULL || to_string->buf == from_string->buf,
           "to_string already exists") ;
  HQASSERT(to_string->length == 0 || to_string->length == from_string->length,
           "to_string already exists") ;

  if (from_string->buf != NULL) {
    if ((to_string->buf = mm_alloc(mm_pcl_pool, from_string->length,
                                    MM_ALLOC_CLASS_PCL_CONTEXT)) == NULL) {
      return FALSE ;
    }

    HqMemCpy(to_string->buf, from_string->buf, from_string->length) ;
    to_string->length = from_string->length ;
  }

  return TRUE ;
}

static void string_free_entry(PCL5StringIdCacheEntry *entry)
{
  size_t size = sizeof(PCL5StringIdCacheEntry) + entry->resource.detail.string_id.length ;

  mm_free(mm_pcl_pool, entry, size) ;
}

/* Constants for PJW hash function. */
#define PJW_SHIFT        (4)            /* Per hashed char hash shift */
#define PJW_MASK         (0xf0000000u)  /* Mask for hash top bits */
#define PJW_RIGHT_SHIFT  (24)           /* Right shift distance for hash top bits */

/* Compute a hash on a string. This is an implementation of hashpjw
 * without any branches in the loop. */
static uint32 hash_string(uint8* ch, int32 length)
{
  uint32 hash;
  uint32 bits;

  HQASSERT(length > 0,
           "hash_string: invalid string length") ;
  HQASSERT(ch != NULL,
           "hash_string: NULL string pointer") ;

  bits = 0;
  hash = 0;
  while (length-- > 0) {
    hash = (hash << PJW_SHIFT) + *ch++;
    bits &= PJW_MASK;
    hash ^= bits | (bits >> PJW_RIGHT_SHIFT);
  }

  hash %= STRING_ID_CACHE_TABLE_SIZE;

  return (hash);
}

/* Compute a hash on a macro string id. */
static uint32 macro_string_hash(pcl5_resource_string_id *string_id)
{
  HQASSERT(string_id != NULL,
           "macro_string_hash: NULL string_id pointer") ;

  return hash_string(string_id->buf, string_id->length);
}

static Bool pcl5_string_id_cache_find_hash(PCL5StringIdCache *string_id_cache,
                                           pcl5_resource_string_id *string_id, uint32 hash,
                                      PCL5StringIdCacheEntry **entry, PCL5StringIdCacheEntry **prev)
{
  PCL5StringIdCacheEntry *curr, *temp_prev = NULL ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;
  HQASSERT(entry != NULL, "entry is NULL") ;
  HQASSERT(prev != NULL, "prev is NULL") ;

  *prev = NULL ;
  *entry = NULL ;

  for (curr = string_id_cache->table[hash]; curr != NULL; curr = curr->next) {
    if (curr->resource.detail.string_id.length == string_id->length &&
        HqMemCmp(curr->resource.detail.string_id.buf, curr->resource.detail.string_id.length,
                 string_id->buf, string_id->length) == 0) {
      *entry = curr ;
      *prev = temp_prev ;
      return TRUE ;
    }
    temp_prev = curr ;
  }

  return FALSE ;
}

static Bool pcl5_string_id_cache_find(PCL5StringIdCache *string_id_cache, pcl5_resource_string_id *string_id,
                                      PCL5StringIdCacheEntry **entry, PCL5StringIdCacheEntry **prev, uint32 *hash)
{
  *hash = macro_string_hash(string_id);
  return (pcl5_string_id_cache_find_hash(string_id_cache, string_id, *hash, entry, prev));
}

static void macro_string_free_entry(PCL5ResourceCaches *resource_caches, PCL5StringIdCacheEntry *entry)
{
  pcl5_macro *macro;

  HQASSERT(entry != NULL, "macro_string_free_entry: NULL entry pointer");

  macro = &entry->resource.macro;

  if (macro->alias != NULL) {
    /* Break any associated id pointers */
    if (macro->alias->detail.private_data == NULL) {
      /* No macro being shadowed, remove proxy */
      pcl5_macro_id_cache_remove(resource_caches->macro, macro->detail.numeric_id);
    } else {
      macro->alias->alias = NULL;
    }
  }
  if (macro->detail.private_data != NULL) {
    /* Release macro data storage */
    macro_free_data(macro);
  }

  mm_free(mm_pcl_pool, entry, sizeof(PCL5StringIdCacheEntry) + macro->detail.string_id.length);
}

static void pcl5_macro_string_id_cache_remove_hash(PCL5ResourceCaches *resource_caches,
                                 pcl5_resource_string_id *string_id, uint32 hash)
{
  PCL5StringIdCacheEntry *entry, *prev ;

  HQASSERT(resource_caches != NULL, "pcl5_macro_string_id_cache_remove_hash: NULL resource_caches pointer") ;

  if (pcl5_string_id_cache_find_hash(resource_caches->string_macro, string_id, hash, &entry, &prev)) {
    /* Unlink. */
    if (prev == NULL) {
      resource_caches->string_macro->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }

    macro_string_free_entry(resource_caches, entry) ;
  }
}

void pcl5_macro_string_id_cache_remove(PCL5ResourceCaches *resource_caches,
                                 pcl5_resource_string_id *string_id)
{
  uint32 hash;

  hash = macro_string_hash(string_id);
  pcl5_macro_string_id_cache_remove_hash(resource_caches, string_id, hash);
}

void pcl5_string_id_cache_remove(PCL5StringIdCache *string_id_cache,
                                 pcl5_resource_string_id *string_id)
{
  PCL5StringIdCacheEntry *entry, *prev ;
  uint32 hash ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;

  if (pcl5_string_id_cache_find(string_id_cache, string_id, &entry, &prev, &hash)) {
    /* Unlink. */
    if (prev == NULL) {
      string_id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }

    string_free_entry(entry);
  }
}

/* Remove all temporary entries from the passed cache. If
 * 'include_permanent' is TRUE, permanent entries will be removed too.
 */
void pcl5_string_id_cache_remove_all(PCL5StringIdCache *string_id_cache,
                                     Bool include_permanent)
{
  int32  i ;
  PCL5StringIdCacheEntry *curr, *next_entry, *prev ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;

  /* If we really need a faster way to iterate over entires we should
     maintain a stack of them. */
  for (i=0; i<STRING_ID_CACHE_TABLE_SIZE; i++) {
    curr = string_id_cache->table[i] ;
    prev = NULL ;
    while (curr != NULL) {
      next_entry = curr->next ;

      if (include_permanent || ! curr->resource.detail.permanent) {
        if (prev == NULL) {
          string_id_cache->table[i] = curr->next ;
        } else {
          prev->next = curr->next ;
        }
        string_free_entry(curr) ;
      } else {
        prev = curr ;
      }

      curr = next_entry ;
    }
  }
}

/* Remove all temporary entries from the passed cache. If
 * 'include_permanent' is TRUE, permanent entries will be removed too.
 */
void pcl5_macro_string_id_cache_remove_all(PCL5ResourceCaches *resource_caches, Bool include_permanent)
{
  PCL5StringIdCacheEntry **table;
  PCL5StringIdCacheEntry *curr, *next_entry, *prev ;
  int32  i ;

  HQASSERT(resource_caches != NULL, "pcl5_macro_string_id_cache_remove_all: NULL resource_caches pointer") ;

  /* If we really need a faster way to iterate over entires we should
     maintain a stack of them. */
  table = resource_caches->string_macro->table;
  for ( i = 0; i < STRING_ID_CACHE_TABLE_SIZE; i++) {
    prev = NULL ;
    for (curr = table[i]; curr != NULL; curr = next_entry) {
      next_entry = curr->next ;
      if (!curr->resource.detail.permanent || include_permanent) {
        if (prev == NULL) {
          table[i] = curr->next ;
        } else {
          prev->next = curr->next ;
        }
        macro_string_free_entry(resource_caches, curr) ;
      } else {
        prev = curr ;
      }
    }
  }
}

pcl5_macro* pcl5_string_id_cache_get_macro(PCL5StringIdCache *string_id_cache, pcl5_resource_string_id *string_id)
{
  PCL5StringIdCacheEntry *entry, *prev ;
  uint32 hash ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;
  HQASSERT(string_id != NULL, "string is NULL") ;

  if (pcl5_string_id_cache_find(string_id_cache, string_id, &entry, &prev, &hash))
    return &(entry->resource.macro) ;

  return NULL ;
}

pcl5_font* pcl5_string_id_cache_get_font(PCL5StringIdCache *string_id_cache, pcl5_resource_string_id *string_id)
{
  PCL5StringIdCacheEntry *entry, *prev ;
  uint32 hash ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;

  if (pcl5_string_id_cache_find(string_id_cache, string_id, &entry, &prev, &hash))
    return &(entry->resource.aliased_font) ;

  return NULL ;
}

Bool pcl5_string_id_cache_insert_macro(PCL5ResourceCaches *resource_caches,
                                       pcl5_resource_string_id *string_id,
                                       pcl5_macro *macro, pcl5_macro **new_macro)
{
  PCL5StringIdCacheEntry *entry;
  uint32 hash;

  *new_macro = NULL ;

  hash = macro_string_hash(string_id);
  pcl5_macro_string_id_cache_remove_hash(resource_caches, string_id, hash);

  entry = mm_alloc(mm_pcl_pool, sizeof(PCL5StringIdCacheEntry) + string_id->length, MM_ALLOC_CLASS_PCL_CONTEXT);
  if (entry == NULL) {
    return FALSE ;
  }

  entry->next = resource_caches->string_macro->table[hash] ;
  resource_caches->string_macro->table[hash] = entry ;

  entry->resource.macro = *macro ;
  entry->resource.detail.string_id.buf = (uint8*)(entry + 1);
  entry->resource.detail.string_id.length = string_id->length ;
  HqMemCpy(entry->resource.detail.string_id.buf, string_id->buf, string_id->length) ;
  entry->resource.macro.alias = NULL ;

  *new_macro = &(entry->resource.macro) ;
  return TRUE ;
}

Bool pcl5_string_id_cache_insert_font(PCL5StringIdCache *string_id_cache,
                                      pcl5_resource_string_id *string_id,
                                      pcl5_font *font, pcl5_font **new_font)
{
  PCL5StringIdCacheEntry *entry, *prev ;
  uint32 hash ;

  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;

  *new_font = NULL ;

  if (pcl5_string_id_cache_find(string_id_cache, string_id, &entry, &prev, &hash)) {
    if (prev == NULL) {
      string_id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }
    string_free_entry(entry) ;
  }

  if ((entry = mm_alloc(mm_pcl_pool, sizeof(PCL5StringIdCacheEntry) + string_id->length, MM_ALLOC_CLASS_PCL_CONTEXT)) == NULL) {
    return FALSE ;
  } else {
    entry->next = string_id_cache->table[hash] ;
    string_id_cache->table[hash] = entry ;
  }

  entry->resource.aliased_font = *font ;
  entry->resource.detail.resource_type = SW_PCL5_FONT ;
  entry->resource.detail.string_id.buf = (uint8*)entry + sizeof(PCL5StringIdCacheEntry) ;
  entry->resource.detail.string_id.length = string_id->length ;
  HqMemCpy(entry->resource.detail.string_id.buf, string_id->buf, string_id->length) ;
  entry->resource.aliased_font.alias = NULL ;

  *new_font = &(entry->resource.aliased_font) ;
  return TRUE ;
}

Bool pcl5_string_id_cache_insert_aliased_font(PCL5StringIdCache *string_id_cache,
                                              pcl5_font *orig_font,
                                              pcl5_resource_string_id *string_id)
{
  pcl5_font font, *new_font ;
  Bool success ;

  font.detail.resource_type = SW_PCL5_FONT ;
  font.detail.numeric_id = 0 ;
  font.detail.string_id.buf = NULL ;
  font.detail.string_id.length = 0 ;
  font.detail.permanent = orig_font->detail.permanent ;
  /* Point to the original macro data. */
  font.detail.private_data = orig_font->detail.private_data ;

  success = pcl5_string_id_cache_insert_font(string_id_cache, string_id, &font, &new_font) ;
  /*
  orig_font->alias = new_font ;
  */
  new_font->alias = orig_font ;

  return success ;
}

void pcl5_string_id_cache_set_permanent(PCL5StringIdCache *string_id_cache, pcl5_resource_string_id *string_id, Bool permanent)
{
  PCL5StringIdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_string_id_cache_find(string_id_cache, string_id, &entry, &prev, &hash)) {
    entry->resource.detail.permanent = permanent ;
  }
}

static
Bool pcl5_string_id_cache_create(PCL5StringIdCache **string_id_cache, uint32 table_size)
{
  PCL5StringIdCache *new_string_id_cache ;
  int32 i ;

  UNUSED_PARAM(uint32, table_size) ;
  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;

  if ((new_string_id_cache = mm_alloc(mm_pcl_pool, sizeof(PCL5StringIdCache), MM_ALLOC_CLASS_PCL_CONTEXT)) == NULL) {
    *string_id_cache = NULL ;
    return FALSE ;
  }
  for (i=0; i<STRING_ID_CACHE_TABLE_SIZE; i++) {
    new_string_id_cache->table[i] = NULL ;
  }

#if 0
  TBD
  new_string_id_cache->table_size = table_size ;
#else
  new_string_id_cache->table_size = STRING_ID_CACHE_TABLE_SIZE ;
#endif

  *string_id_cache = new_string_id_cache ;

  return TRUE ;
}

void pcl5_string_id_cache_destroy(PCL5StringIdCache **string_id_cache)
{
  int32  i ;
  PCL5StringIdCacheEntry *curr, *next_entry ;
  HQASSERT(string_id_cache != NULL, "string_id_cache is NULL") ;
  HQASSERT(*string_id_cache != NULL, "*string_id_cache is NULL") ;

  /* If we really need a faster way to iterate over entires we should
     maintain a stack of them. */
  for (i=0; i<STRING_ID_CACHE_TABLE_SIZE; i++) {
    curr = (*string_id_cache)->table[i] ;
    while (curr != NULL) {
      next_entry = curr->next ;
      string_free_entry(curr) ;
      curr = next_entry ;
    }
  }

  mm_free(mm_pcl_pool, *string_id_cache, sizeof(PCL5StringIdCache)) ;
  *string_id_cache = NULL ;
}

/* ============================================================================
 * Numeric ID cache for patterns and macros
 * ============================================================================
 */

#define ID_CACHE_TABLE_SIZE 37

/* Because pcl5_resource is ALWAYS the first field of specfic resource
   type, we can simply use resource to check its type, id etc.. */
struct PCL5IdCacheEntry {
  union {
    pcl5_resource detail ;
    pcl5_pattern pattern ;
    pcl5_macro macro ;
    pcl5_font aliased_font ;
  } resource ;

  struct PCL5IdCacheEntry *next ;
} ;

struct PCL5IdCache {
  PCL5IdCacheEntry* table[ID_CACHE_TABLE_SIZE] ;
  PCL5IdCacheEntry* zombies ;
  uint32 table_size ;
} ;

/**
 * Free the raster data in the passed pattern.
 */
static void free_pattern_data(pcl5_pattern *pattern)
{
  if (pattern->data != NULL) {
    uint32 size = pcl5_id_cache_pattern_data_size(pattern->width,
                                                  pattern->height,
                                                  pattern->bits_per_pixel) ;
    mm_free(mm_pcl_pool, pattern->data, size) ;
    pattern->data = NULL;
  }
}

static void free_entry(PCL5IdCacheEntry *entry)
{
  if (entry->resource.detail.resource_type == SW_PCL5_PATTERN) {
    free_pattern_data(&entry->resource.pattern);
  }

  mm_free(mm_pcl_pool, entry, sizeof(PCL5IdCacheEntry)) ;
}

static void macro_free_entry(PCL5IdCacheEntry *entry)
{
  /* Break any associated id pointers */
  if (entry->resource.macro.alias != NULL) {
    entry->resource.macro.alias->alias = NULL;
  }
  /* Release macro data storage */
  if (entry->resource.detail.private_data != NULL) {
    macro_free_data(&entry->resource.macro);
  }
  /* Release macro cache entry */
  mm_free(mm_pcl_pool, entry, sizeof(PCL5IdCacheEntry)) ;
}

static Bool pcl5_id_cache_find(PCL5IdCache *id_cache, pcl5_resource_numeric_id id, PCL5IdCacheEntry **entry,
                               PCL5IdCacheEntry **prev, uint32 *hash)
{
  PCL5IdCacheEntry *curr, *temp_prev = NULL ;

  HQASSERT(id_cache != NULL, "id_cache is NULL") ;
  HQASSERT(entry != NULL, "entry is NULL") ;
  HQASSERT(hash != NULL, "hash is NULL") ;

  *hash = id % ID_CACHE_TABLE_SIZE ;
  *prev = NULL ;
  *entry = NULL ;

  for (curr = id_cache->table[*hash]; curr != NULL; curr = curr->next) {
    if (curr->resource.detail.numeric_id == id) {
      *entry = curr ;
      *prev = temp_prev ;
      return TRUE ;
    }
    temp_prev = curr ;
  }

  return FALSE ;
}

void make_zombie(PCL5IdCache *id_cache, PCL5IdCacheEntry *entry)
{
  HQASSERT(id_cache != NULL, "id_cache is NULL") ;
  HQASSERT(entry != NULL, "entry is NULL") ;

  entry->next = id_cache->zombies ;
  id_cache->zombies = entry ;
}

void pcl5_id_cache_remove(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash)) {
    /* Unlink. */
    if (prev == NULL) {
      id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }
    make_zombie(id_cache, entry) ;
  }
}

void pcl5_macro_id_cache_remove(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash)) {
    if (prev == NULL) {
      id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }
    macro_free_entry(entry);
  }
}

void pcl5_id_cache_kill_zombies(PCL5IdCache *id_cache)
{
  PCL5IdCacheEntry *curr ;

  curr = id_cache->zombies ;
  while (curr != NULL) {
    id_cache->zombies = curr->next ;
    free_entry(curr) ;
    curr = id_cache->zombies ;
  }
}

/* Remove numeric id macros created by association only */
void pcl5_macro_id_cache_remove_assoc(PCL5IdCache *id_cache)
{
  PCL5IdCacheEntry *curr;
  PCL5IdCacheEntry **pcurr;
  pcl5_macro *macro;
  int32 i;

  HQASSERT(id_cache != NULL,
           "pcl5_macro_id_cache_remove_assoc: NULL id_cache pointer");

  for (i = 0; i < ID_CACHE_TABLE_SIZE; i++) {
    pcurr = &id_cache->table[i];
    for (curr = *pcurr; curr != NULL; curr = *pcurr) {
      macro = &curr->resource.macro;
      if (macro->alias != NULL) {
        macro->alias->alias = NULL;
        macro->alias = NULL;
      }
      if (macro->detail.private_data == NULL) {
        *pcurr = curr->next;
        macro_free_entry(curr);
      } else {
        pcurr = &curr->next;
      }
    }
  }
}

/* Remove all temporary entries from the passed cache. If
 * 'include_permanent' is TRUE, permanent entries will be removed too.
 */
void pcl5_macro_id_cache_remove_all(PCL5IdCache *id_cache, Bool include_permanent)
{
  PCL5IdCacheEntry *curr, *next_entry, *prev;
  int32 i;

  for ( i = 0; i < ID_CACHE_TABLE_SIZE; i++) {
    prev = NULL ;
    for (curr = id_cache->table[i]; curr != NULL; curr = next_entry) {
      next_entry = curr->next;
      if (!curr->resource.detail.permanent || include_permanent) {
        if (prev == NULL) {
          id_cache->table[i] = curr->next ;
        } else {
          prev->next = curr->next ;
        }
        macro_free_entry(curr);
      } else {
        prev = curr ;
      }
    }
  }
}

/* Remove all temporary entries from the passed cache. If
 * 'include_permanent' is TRUE, permanent entries will be removed too.
 */
void pcl5_id_cache_remove_all(PCL5IdCache *id_cache, Bool include_permanent)
{
  int32  i ;
  PCL5IdCacheEntry *curr, *next_entry, *prev ;
  HQASSERT(id_cache != NULL, "id_cache is NULL") ;

  /* If we really need a faster way to iterate over entires we should
     maintain a stack of them. */
  for (i=0; i<ID_CACHE_TABLE_SIZE; i++) {
    curr = id_cache->table[i] ;
    prev = NULL ;
    while (curr != NULL) {
      next_entry = curr->next ;

      if (include_permanent || ! curr->resource.detail.permanent) {
        if (prev == NULL) {
          id_cache->table[i] = curr->next ;
        } else {
          prev->next = curr->next ;
        }
        make_zombie(id_cache, curr) ;
      } else {
        prev = curr ;
      }

      curr = next_entry ;
    }
  }
}

/* Data to store and order:
 * . numeric id - 16bit
 * . width - 16bit
 * . height - 16bit
 * . xdpi - 16bit (always set)
 * . ydpi - 16bit (always set)
 * . color - 8bit (0 for mono)
 * . bpp - 8bit (1 for mono)
 * . stride - 16bit
 * . pattern data ...
 */
#define PCL5_UDP_ID       (0)
#define PCL5_UDP_WIDTH    (2)
#define PCL5_UDP_HEIGHT   (4)
#define PCL5_UDP_XDPI     (6)
#define PCL5_UDP_YDPI     (8)
#define PCL5_UDP_COLOR    (10)
#define PCL5_UDP_BPP      (11)
#define PCL5_UDP_STRIDE   (12)
#define PCL5_UDP_HDRLEN   (14)

static Bool permres_store_pattern(
  struct PCL_RESOURCE_HANDLE *handle,
  pcl5_pattern  *pattern)
{
  uint8 header[PCL5_UDP_HDRLEN];
  size_t datalen;
  size_t bytes;

  HQASSERT(handle != NULL,
           "permres_store_pattern: NULL resource handle");
  HQASSERT(pattern != NULL,
           "permres_store_pattern: NULL pattern pointer");

  BYTE_STORE16_LE(&header[PCL5_UDP_ID], pattern->detail.numeric_id);
  BYTE_STORE16_LE(&header[PCL5_UDP_WIDTH], pattern->width);
  BYTE_STORE16_LE(&header[PCL5_UDP_HEIGHT], pattern->height);
  BYTE_STORE16_LE(&header[PCL5_UDP_XDPI], pattern->x_dpi);
  BYTE_STORE16_LE(&header[PCL5_UDP_YDPI], pattern->y_dpi);
  header[PCL5_UDP_COLOR] = CAST_SIGNED_TO_UINT8(pattern->color);
  header[PCL5_UDP_BPP] = CAST_SIGNED_TO_UINT8(pattern->bits_per_pixel);
  BYTE_STORE16_LE(&header[PCL5_UDP_STRIDE], pattern->stride);

  bytes = pcl5permres_api->transfer(handle, header, PCL5_UDP_HDRLEN);
  if (bytes < PCL5_UDP_HDRLEN) {
    return (FALSE);
  }

  datalen = pattern->height*pattern->stride;
  bytes = pcl5permres_api->transfer(handle, pattern->data, datalen);
  if (bytes < datalen) {
    return (FALSE);
  }
  return (TRUE);
}

static Bool permres_load_pattern(
  PCL5IdCache* user,
  struct PCL_RESOURCE_HANDLE *handle)
{
  pcl5_pattern pattern;
  uint8 header[PCL5_UDP_HDRLEN];
  pcl5_pattern *new_pattern;
  size_t datalen;
  size_t bytes;

  HQASSERT(user != NULL,
           "permres_load_pattern: NULL udp cache pointer");
  HQASSERT(handle != NULL,
           "permres_load_pattern: NULL permres api pointer");

  bytes = pcl5permres_api->transfer(handle, header, PCL5_UDP_HDRLEN);
  if (bytes < PCL5_UDP_HDRLEN) {
    return (FALSE);
  }

  pattern.detail.resource_type = SW_PCL5_PATTERN;
  pattern.detail.numeric_id = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_ID]);
  pattern.detail.permanent = TRUE;
  pattern.detail.private_data = NULL;

  pattern.width = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_ID]);
  pattern.height = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_HEIGHT]);
  pattern.x_dpi = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_XDPI]);
  pattern.y_dpi = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_YDPI]);
  pattern.color = header[PCL5_UDP_COLOR] != 0;
  pattern.highest_pen = 0;
  pattern.bits_per_pixel = header[PCL5_UDP_BPP];
  pattern.stride = BYTE_LOAD16_UNSIGNED_LE(&header[PCL5_UDP_STRIDE]);
  pattern.data = NULL;

  if (!pcl5_id_cache_insert_pattern(user, pattern.detail.numeric_id,
                                    &pattern, &new_pattern)) {
    return (FALSE);
  }

  datalen = pattern.height*pattern.stride;
  /* TODO - all black patterns have no pattern data - special case. */
  bytes = pcl5permres_api->transfer(handle, new_pattern->data, datalen);
  if (bytes < datalen) {
    return (FALSE);
  }

  return (TRUE);
}

void pcl5_id_cache_load_permanent_patterns(
  PCL5IdCache *udp_cache)
{
  struct PCL_RESOURCE_HANDLE *handle;
  Bool load_ok;

  HQASSERT(udp_cache != NULL,
           "pcl5_id_cache_load_permanent_patterns: NULL udp_cache pointer");

  if (pcl5permres_api == NULL) {
    return;
  }

  handle = pcl5permres_api->start(PCL5_RESOURCE_UDP, PCL5_RESOURCE_LOAD);
  if (handle == NULL) {
    return;
  }

  load_ok = FALSE;
  do {
    load_ok = permres_load_pattern(udp_cache, handle);
  } while (load_ok);

  pcl5permres_api->finish(&handle);
}

void pcl5_id_cache_store_permanent_patterns(
  PCL5IdCache *udp_cache)
{
  struct PCL_RESOURCE_HANDLE *handle;
  PCL5IdCacheEntry *entry;
  int i;

  HQASSERT(udp_cache != NULL,
           "pcl5_id_cache_store_permanent_patterns: NULL udp_cache pointer");

  if (pcl5permres_api == NULL) {
    return;
  }

  handle = pcl5permres_api->start(PCL5_RESOURCE_UDP, PCL5_RESOURCE_STORE);
  if (handle == NULL) {
    return;
  }

  for (i = 0; i < ID_CACHE_TABLE_SIZE; i++) {
    for (entry = udp_cache->table[i]; entry != NULL; entry = entry->next) {
      if (!entry->resource.detail.permanent) {
        continue;
      }
      if (!permres_store_pattern(handle, &entry->resource.pattern)) {
        break;
      }
    }
  }

  pcl5permres_api->finish(&handle);
}

void pcl5_id_cache_load_permanent_macros(PCL5ResourceCaches *resource_caches)
{
  struct PCL_RESOURCE_HANDLE *handle;
  Bool load_ok;

  if (pcl5permres_api == NULL) {
    return;
  }

  handle = pcl5permres_api->start(PCL5_RESOURCE_MACRO, PCL5_RESOURCE_LOAD);
  if (handle == NULL) {
    return;
  }

  load_ok = FALSE;
  do {
    load_ok = pcl5_macro_load_permanent(resource_caches, handle);
  } while (load_ok);

  pcl5permres_api->finish(&handle);
}

void pcl5_id_cache_store_permanent_macros(
  PCL5ResourceCaches *resource_caches)
{
  pcl5_macro *macro;
  struct PCL_RESOURCE_HANDLE *handle;
  PCL5IdCacheEntry *entry;
  PCL5StringIdCacheEntry *string_entry;
  int i;

  HQASSERT(resource_caches != NULL,
           "pcl5_id_cache_store_permanent_macros: NULL resource_caches pointer");

  if (pcl5permres_api == NULL) {
    return;
  }

  handle = pcl5permres_api->start(PCL5_RESOURCE_MACRO, PCL5_RESOURCE_STORE);
  if (handle == NULL) {
    return;
  }

  for (i = 0; i < ID_CACHE_TABLE_SIZE; i++) {
    for (entry = resource_caches->macro->table[i]; entry != NULL; entry = entry->next) {
      macro = &entry->resource.macro;
      if (macro->detail.permanent && macro->alias == NULL) {
        pcl5_macro_store_permanent(handle, macro);
      }
    }
  }

  for (i = 0; i < STRING_ID_CACHE_TABLE_SIZE; i++) {
    for (string_entry = resource_caches->string_macro->table[i]; string_entry != NULL; string_entry = string_entry->next) {
      if (string_entry->resource.detail.permanent) {
        pcl5_macro_store_permanent(handle, &string_entry->resource.macro);
      }
    }
  }

  pcl5permres_api->finish(&handle);
}

Bool pcl5_id_cache_insert_pattern(PCL5IdCache *id_cache, pcl5_resource_numeric_id id,
                                  pcl5_pattern *pattern,
                                  pcl5_pattern **new_pattern)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;
  int32 data_size ;
  uint8* pattern_data ;

  *new_pattern = NULL ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash)) {
    if (prev == NULL) {
      id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }
    make_zombie(id_cache, entry) ;
  }

  entry = mm_alloc(mm_pcl_pool, sizeof(PCL5IdCacheEntry),
                   MM_ALLOC_CLASS_PCL_CONTEXT);
  if (entry == NULL)
    return FALSE;

  /* Allocate memory for the pattern data; this will be populated by the client. */
  data_size = pcl5_id_cache_pattern_data_size(pattern->width, pattern->height,
                                              pattern->bits_per_pixel) ;
  pattern_data = mm_alloc(mm_pcl_pool, data_size, MM_ALLOC_CLASS_PCL_CONTEXT) ;
  if (pattern_data == NULL) {
    mm_free(mm_pcl_pool, entry, sizeof(PCL5IdCacheEntry)) ;
    return FALSE ;
  }

  entry->next = id_cache->table[hash] ;
  id_cache->table[hash] = entry ;

  entry->resource.pattern = *pattern ;

  HQASSERT(entry->resource.detail.resource_type == SW_PCL5_PATTERN &&
           entry->resource.detail.private_data == NULL,
           "Cache entry incorrectly configured.");

  entry->resource.pattern.data = pattern_data;
  /* Copy the original pattern data in if present. */
  if (pattern->data != NULL)
    HqMemCpy(pattern_data, pattern->data, data_size);

  *new_pattern = &(entry->resource.pattern) ;
  return TRUE ;
}

/* See header for doc. */
void pcl5_id_cache_release_pattern_data(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  pcl5_pattern* pattern = pcl5_id_cache_get_pattern(id_cache, id);
  if (pattern != NULL) {
    free_pattern_data(pattern);
  }
}

pcl5_pattern* pcl5_id_cache_get_pattern(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash))
    return &(entry->resource.pattern) ;

  return NULL ;
}

pcl5_macro* pcl5_id_cache_get_macro(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash))
    return &(entry->resource.macro) ;

  return NULL ;
}

pcl5_font* pcl5_id_cache_get_font(PCL5IdCache *id_cache, pcl5_resource_numeric_id id)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash))
    return &(entry->resource.aliased_font) ;

  return NULL ;
}

pcl5_macro *pcl5_id_cache_insert_macro(PCL5IdCache *id_cache, pcl5_resource_numeric_id id, pcl5_macro *macro)
{
  PCL5IdCacheEntry *entry;
  uint32 hash;

  HQASSERT(pcl5_id_cache_get_macro(id_cache, id) == NULL,
           "pcl5_id_cache_insert_macro: macro with id currently exists");

  entry = mm_alloc(mm_pcl_pool, sizeof(PCL5IdCacheEntry), MM_ALLOC_CLASS_PCL_CONTEXT);
  if (entry == NULL) {
    return (FALSE);
  }

  hash = id % ID_CACHE_TABLE_SIZE;
  entry->next = id_cache->table[hash];
  id_cache->table[hash] = entry;

  entry->resource.macro = *macro;
  entry->resource.detail.resource_type = SW_PCL5_MACRO;
  entry->resource.detail.numeric_id = id;
  entry->resource.macro.alias = NULL;

  return (&entry->resource.macro);
}

Bool pcl5_id_cache_insert_font(PCL5IdCache *id_cache, pcl5_resource_numeric_id id, pcl5_font *font, pcl5_font **new_font)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  *new_font = NULL ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash)) {
    if (prev == NULL) {
      id_cache->table[hash] = entry->next ;
    } else {
      prev->next = entry->next ;
    }
    make_zombie(id_cache, entry) ;
  }

  if ((entry = mm_alloc(mm_pcl_pool, sizeof(PCL5IdCacheEntry), MM_ALLOC_CLASS_PCL_CONTEXT)) == NULL) {
    return FALSE ;
  } else {
    entry->next = id_cache->table[hash] ;
    id_cache->table[hash] = entry ;
  }

  entry->resource.aliased_font = *font ;
  entry->resource.detail.resource_type = SW_PCL5_FONT ;
  entry->resource.aliased_font.alias = NULL ;

  *new_font = &(entry->resource.aliased_font) ;
  return TRUE ;
}

Bool pcl5_id_cache_insert_aliased_font(PCL5IdCache *id_cache, pcl5_font *orig_font, pcl5_resource_numeric_id id)
{
  pcl5_font font, *new_font ;
  Bool success ;

  font.detail.resource_type = SW_PCL5_FONT ;
  font.detail.numeric_id = id ;
  font.detail.string_id.buf = NULL ;
  font.detail.string_id.length = 0 ;
  font.detail.permanent = orig_font->detail.permanent ;
  /* Point to the original macro data. */
  font.detail.private_data = orig_font->detail.private_data ;

  success = pcl5_id_cache_insert_font(id_cache, id, &font, &new_font) ;

  /*
  orig_font->alias = new_font ;
  */
  new_font->alias = orig_font ;

  return success ;
}

uint32 pcl5_id_cache_pattern_data_size(int32 width, int32 height, int32 bitsPerPixel)
{
  HQASSERT(bitsPerPixel == 1 || bitsPerPixel == 8, "Invalid bitsPerPixel.");
  if (bitsPerPixel == 1)
    return ((width + 7) / 8) * height;
  else
    return width * height;
}

void pcl5_id_cache_set_permanent(PCL5IdCache *id_cache, pcl5_resource_numeric_id id, Bool permanent)
{
  PCL5IdCacheEntry *entry, *prev ;
  uint32 hash ;

  if (pcl5_id_cache_find(id_cache, id, &entry, &prev, &hash)) {
    entry->resource.detail.permanent = permanent ;
  }
}

static
Bool pcl5_id_cache_create(PCL5IdCache **id_cache, uint32 table_size)
{
  PCL5IdCache *new_id_cache ;
  int32 i ;

  UNUSED_PARAM(uint32, table_size) ;
  HQASSERT(id_cache != NULL, "id_cache is NULL") ;

  if ((new_id_cache = mm_alloc(mm_pcl_pool, sizeof(PCL5IdCache), MM_ALLOC_CLASS_PCL_CONTEXT)) == NULL) {
    *id_cache = NULL ;
    return FALSE ;
  }
  for (i=0; i<ID_CACHE_TABLE_SIZE; i++) {
    new_id_cache->table[i] = NULL ;
  }

  new_id_cache->zombies = NULL ;
#if 0
  TBD
  new_id_cache->table_size = table_size ;
#else
  new_id_cache->table_size = ID_CACHE_TABLE_SIZE ;
#endif

  *id_cache = new_id_cache ;

  return TRUE ;
}

void pcl5_id_cache_destroy(PCL5IdCache **id_cache)
{
  int32  i ;
  PCL5IdCacheEntry *curr, *next_entry ;
  HQASSERT(id_cache != NULL, "id_cache is NULL") ;
  HQASSERT(*id_cache != NULL, "*id_cache is NULL") ;

  pcl5_id_cache_kill_zombies(*id_cache) ;

  /* If we really need a faster way to iterate over entires we should
     maintain a stack of them. */
  for (i=0; i<ID_CACHE_TABLE_SIZE; i++) {
    curr = (*id_cache)->table[i] ;
    while (curr != NULL) {
      next_entry = curr->next ;
      free_entry(curr) ;
      curr = next_entry ;
    }
  }

  mm_free(mm_pcl_pool, *id_cache, sizeof(PCL5IdCache)) ;
  *id_cache = NULL ;
}

/* See header for doc. */
void pcl5_id_cache_start_interation(PCL5IdCache* id_cache,
                                    PCL5IdCacheIterator* iterator)
{
  iterator->cache = id_cache;
  iterator->index = -1;
  iterator->entry = NULL;
}

/* See header for doc. */
pcl5_resource* pcl5_id_cache_iterate(PCL5IdCacheIterator* iterator)
{
  PCL5IdCache* cache = iterator->cache;

  /* Move on to the next entry in the linked list. */
  if (iterator->entry != NULL)
    iterator->entry = iterator->entry->next;

  /* If the entry is null, find the next occupied table entry. */
  if (iterator->entry == NULL) {
    do {
      iterator->index ++;
    } while (CAST_SIGNED_TO_UINT32(iterator->index) < cache->table_size &&
             cache->table[iterator->index] == NULL);

    if (CAST_SIGNED_TO_UINT32(iterator->index) < cache->table_size)
      iterator->entry = cache->table[iterator->index];
  }

  return &iterator->entry->resource.detail;
}

/* ============================================================================
 * Init/Finish PCL5 resource caches.
 * ============================================================================
 */
void pcl5_resource_caches_finish(PCL5_RIP_LifeTime_Context *pcl5_rip_context)
{
  HQASSERT(pcl5_rip_context != NULL, "pcl5_rip_context is NULL") ;

  destroy_pattern_caches(pcl5_rip_context) ;

  if (pcl5_rip_context->resource_caches.shading != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.shading)) ;

  if (pcl5_rip_context->resource_caches.cross_hatch != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.cross_hatch)) ;

  if (pcl5_rip_context->resource_caches.user != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.user)) ;

  if (pcl5_rip_context->resource_caches.hpgl2_user != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.hpgl2_user)) ;

  if (pcl5_rip_context->resource_caches.aliased_string_font != NULL)
    pcl5_string_id_cache_destroy(&pcl5_rip_context->resource_caches.aliased_string_font);

  if (pcl5_rip_context->resource_caches.aliased_font != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.aliased_font)) ;

  if (pcl5_rip_context->resource_caches.string_macro != NULL)
    pcl5_string_id_cache_destroy(&pcl5_rip_context->resource_caches.string_macro);

  if (pcl5_rip_context->resource_caches.macro != NULL)
    pcl5_id_cache_destroy(&(pcl5_rip_context->resource_caches.macro)) ;
}

Bool pcl5_resource_caches_init(PCL5_RIP_LifeTime_Context *pcl5_rip_context)
{

  HQASSERT(pcl5_rip_context != NULL, "pcl5_rip_context is NULL") ;

  pcl5_rip_context->resource_caches.shading = NULL ;
  pcl5_rip_context->resource_caches.cross_hatch = NULL ;
  pcl5_rip_context->resource_caches.user = NULL ;
  pcl5_rip_context->resource_caches.hpgl2_user = NULL ;
  pcl5_rip_context->resource_caches.macro = NULL ;
  pcl5_rip_context->resource_caches.string_macro = NULL ;
  pcl5_rip_context->resource_caches.aliased_font = NULL ;
  pcl5_rip_context->resource_caches.aliased_string_font = NULL ;

  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.shading), 10))
    goto cleanup ;
  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.cross_hatch), 6))
    goto cleanup ;
  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.user), 41))
    goto cleanup ;
  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.hpgl2_user), 8))
    goto cleanup ;
  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.macro), 37))
    goto cleanup ;
  if (! pcl5_string_id_cache_create(&(pcl5_rip_context->resource_caches.string_macro), 37))
    goto cleanup ;
  if (! pcl5_id_cache_create(&(pcl5_rip_context->resource_caches.aliased_font), 37))
    goto cleanup ;
  if (! pcl5_string_id_cache_create(&(pcl5_rip_context->resource_caches.aliased_string_font), 37))
    goto cleanup ;

  if (! init_pattern_caches(pcl5_rip_context))
    goto cleanup ;

  return TRUE ;

 cleanup:
  pcl5_resource_caches_finish(pcl5_rip_context) ;

  return FALSE ;
}

/* ============================================================================
 * Operator callbacks are below here.
 * ============================================================================
 */

/* Alphanumeric ID command. */
Bool pcl5op_ampersand_n_W(PCL5Context *pcl5_ctxt, int32 explicit_sign, PCL5Numeric value)
{
  pcl5_resource_string_id string_id;
  uint8 buffer[MAX_STRING_ID_LENGTH] ;
  MacroInfo *macro_info;
  int32 num_bytes ;
  int32 operation ;

  UNUSED_PARAM(int32, explicit_sign) ;
  HQASSERT(pcl5_ctxt != NULL, "pcl5_ctxt is NULL") ;

  num_bytes = min(value.integer, MAX_STRING_ID_LENGTH + 1);

  /* Data comprises operator byte and possible string */
  if ( num_bytes == 0 ) {
    return(TRUE);
  }

  if ((operation = Getc(pcl5_ctxt->flptr)) == EOF)
    return FALSE ;
  num_bytes-- ;

  string_id.length = num_bytes;
  string_id.buf = buffer;
  /* TODO - add proper checking on length to prevent buffer overflow */
  if ( num_bytes > 0 ) {
    if ( file_read(pcl5_ctxt->flptr, string_id.buf, string_id.length, NULL) <= 0 ) {
      return(FALSE);
    }
  }

  /* See test QL 5e CET 20.04. Seems string lengths are limited to 253
     characters, not 511. Not only that, but it appears that the
     command is ignored if the string length is greater than 253
     characters. */
  if (string_id.length > MAX_STRING_ID_LENGTH)
    return TRUE ;

  /* The only time a string length of zero is allowed is for operators 100, 21 or 20. */
  if (string_id.length == 0 && operation != 100 && operation != 21 && operation != 20)
    return TRUE ;

  /* We have the string length and the string itself. */
  switch (operation) {
  case 0: /* Sets the current Font ID to the given String ID. */
    if (! set_font_string_id(pcl5_ctxt, &string_id))
      return FALSE ;
    break ;
  case 1: /* Associates the current Font ID to the font with the String ID
             supplied. */
    if (! associate_font_string_id(pcl5_ctxt, &string_id))
      return FALSE ;
    break ;
  case 2: /* Selects the font referred to by the String ID as primary. */
    if (! select_font_string_id_as_primary(pcl5_ctxt, &string_id))
      return FALSE ;
    break ;
  case 3: /* Selects the font referred to by the String ID as secondary. */
    if (! select_font_string_id_as_secondary(pcl5_ctxt, &string_id))
      return FALSE ;
    break ;
  case 4: /* Sets the current Macro ID to the String ID. */
    set_macro_string_id(pcl5_ctxt, &string_id);
    break ;
  case 5: /* Associates the current Macro ID to the supplied String ID. */
    macro_info = get_macro_info(pcl5_ctxt);
    associate_macro_string_id(&pcl5_ctxt->resource_caches, macro_info->macro_numeric_id, &string_id);
    break ;
  case 20: /* Deletes the font association named by the current Font ID. */
    if (! delete_font_associated_string_id(pcl5_ctxt))
      return FALSE ;
    break ;
  case 21: /* Deletes the macro association named by the current Macro ID. */
    delete_macro_associated_string_id(pcl5_ctxt);
    break ;
  case 100: /* Media select (see media selection table). */
    if (pcl5_ctxt->pass_through_mode != PCLXL_SNIPPET_JOB_PASS_THROUGH) {
      /* N.B. If no data is supplied in the alphanumeric command this seems
       * equivalent to asking for media type 0 or Plain.
       */
      if (string_id.length > 0) {
        if (! set_media_type_from_alpha(pcl5_ctxt, string_id.buf, string_id.length))
          return FALSE ;
      } else {
        if (! set_media_type(pcl5_ctxt, 0))
          return FALSE ;
      }
    }
    break ;
  }

  return TRUE ;
}

void reset_aliased_fonts(PCL5_RIP_LifeTime_Context *pcl5_rip_context)
{
  HQASSERT(pcl5_rip_context != NULL, "pcl5_rip_context is NULL") ;

  pcl5_string_id_cache_remove_all(pcl5_rip_context->resource_caches.aliased_string_font,
                                  TRUE /* Include permanent? */) ;

  pcl5_id_cache_remove_all(pcl5_rip_context->resource_caches.aliased_font,
                           TRUE /* Include permanent? */) ;
  pcl5_id_cache_kill_zombies(pcl5_rip_context->resource_caches.aliased_font) ;
}


/* ============================================================================
* Log stripped */
