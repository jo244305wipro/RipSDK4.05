/** \file
 * \ingroup interface
 *
 * Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 * Global Graphics Software Ltd. Confidential Information.
 */

#ifndef __PCL5PERMRES_H__
#define __PCL5PERMRES_H__ (1)

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Permanent PCL 5 Resource types. */
enum {
  PCL5_RESOURCE_FONT = 0,
  PCL5_RESOURCE_SYMBOLSET = 1,
  PCL5_RESOURCE_UDP = 2,
  PCL5_RESOURCE_MACRO = 3
};

/* PCL 5 Resource data operations. */
enum {
  PCL5_RESOURCE_LOAD = 0,
  PCL5_RESOURCE_STORE = 1
};

/* PCL 5 Resource API error codes. */
enum {
  PCL5_RESOURCE_OK = 0,
  PCL5_RESOURCE_FAIL = 1
};

typedef struct PCL_RESOURCE_API_20140822 {
  /* Prepare to load or store a PCL 5 Resource type.
   * Returns a valid pointer on success, else NULL. */
  struct PCL_RESOURCE_HANDLE *(HQNCALL *start)(
    int type,
    int operation);

  /* Finish loading or storing PCL 5 resource type data.
   * Returns PCL_RESOURCE_OK on success, else PCL_RESOURCE_FAIL. */
  int (HQNCALL *finish)(
    struct PCL_RESOURCE_HANDLE **handle);

  /* Return detailed error code for last failed operation. */
  int (HQNCALL *error)(
    struct PCL_RESOURCE_HANDLE *handle);

  /* Transfer resource data to or from supplied buffer based on operation passed to
   * start().
   * Returns number of bytes transferred. 0 when no more bytes can be transferred. */
  size_t (HQNCALL *transfer)(
    struct PCL_RESOURCE_HANDLE *handle,
    unsigned char *buffer,
    size_t size);
} PCL_RESOURCE_API_20140822;

#ifdef __cplusplus
}
#endif

#endif /* !__PCL5PERMRES_H__ */
