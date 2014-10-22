/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 */
/*!
 * \file
 * \ingroup OIL
 * \brief This file contains the definitions of the functions to control the
 * loading and storing on PCL 5 permanent resources.
 */

#include "oil.h"
#include "oil_malloc.h"
#include "oil_interface_oil2pms.h"
#include "oil_pcl5permres.h"

#include "pms_export.h"

#include "pcl5permres.h"


/* Maps PCL resource types in core interface to PMS values. */
static int pms_resource_types[4] = {
  PMS_PCL5ResourceFont,
  PMS_PCL5ResourceSymbolSet,
  PMS_PCL5ResourceUDP,
  PMS_PCL5ResourceMacro
};

/* Maps PCL resource operations in core interface to PMS values. */
static int pms_resource_operations[2] = {
  PMS_PCL5ResourceLoad,
  PMS_PCL5ResourceStore
};

/**
 * \brief Return a PCL resource handle ready to transfer resource data.
 */
static struct PCL_RESOURCE_HANDLE * HQNCALL PCL5PermRes_start(int type, int operation)
{
  struct PCL_RESOURCE_HANDLE *handle;

  /* Check type and operation values will be valid indexes when mapping to
   * equivalent PMS values.
   */
  if ((type < PCL5_RESOURCE_FONT || type > PCL5_RESOURCE_MACRO) ||
      (operation != PCL5_RESOURCE_LOAD && operation != PCL5_RESOURCE_STORE)) {
    return (NULL);
  }

  return (PMS_PCL5ResourceStart(pms_resource_types[type],
                                pms_resource_operations[operation],
                                &handle)
            ? handle
            : NULL);
}


/**
 * \brief Finish using a PCL resource handle for transferring resource data.
 */
static int HQNCALL PCL5PermRes_finish(struct PCL_RESOURCE_HANDLE **handle)
{
  return (PMS_PCL5ResourceFinish(handle)
            ? PCL5_RESOURCE_OK
            : PCL5_RESOURCE_FAIL);
}


/* Maps PMS PCL error values to core interface values. */
static int pms_resource_error[2] = {
  PCL5_RESOURCE_OK,
  PCL5_RESOURCE_FAIL
};

/**
 * \brief Return last error for a PCL 5 resource handle.
 */
static int HQNCALL PCL5PermRes_error(struct PCL_RESOURCE_HANDLE *handle)
{
  int error;

  if (PMS_PCL5ResourceError(handle, &error) != 1) {
    return (PCL5_RESOURCE_FAIL);
  }

  return (pms_resource_error[error]);
}


/**
 * \brief Transfer PCL 5 resource data.
 */
static size_t HQNCALL PCL5PermRes_transfer(struct PCL_RESOURCE_HANDLE *handle, unsigned char *buffer, size_t size)
{
  int bytes;

  return (PMS_PCL5ResourceTransfer(handle, buffer, (int)size, &bytes)
            ? bytes
            : 0);
}


static struct PCL_RESOURCE_API_20140822 g_pcl_resource_api = {
  PCL5PermRes_start,
  PCL5PermRes_finish,
  PCL5PermRes_error,
  PCL5PermRes_transfer
};

/**
 * \brief Returns PCL 5 permanent resource RDR API structure.
 */
void *OIL_PCLResource_Interface(int *length)
{
  *length = sizeof(g_pcl_resource_api);
  return (&g_pcl_resource_api);
}

/* EOF */
