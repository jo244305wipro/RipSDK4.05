/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 */

/*! \file
 *  \ingroup PMS
 */

#include <stdio.h>
#include <string.h>

#include "pms.h"
#include "pms_platform.h"
#include "pms_malloc.h"

#include "pms_pcl5permres.h"


/**
 * \brief The PCL 5 resource structure
 */
struct PCL_RESOURCE_HANDLE {
  int operation;
  FILE *resource_file;
  int last_error;
};


static char g_szResourceDir[FILENAME_MAX];

/**
 * \brief Generate a filename for a PCL 5 resource type.
 *
 * The returned file name is valid if the function returns 1.  The actual length
 * of the file name is returned in len.
 */
static int resource_filename(int type, char *buffer, size_t size, size_t *len)
{
  /* Resource type filenames must in same order as PMS enum of resource types */
  static char *type_filenames[4] = {
    "PCLFont.res",
    "PCLSymbolSet.res",
    "PCLUDP.res",
    "PCLMacro.res"
  };

  if (type < PMS_PCL5ResourceFont || type > PMS_PCL5ResourceMacro) {
    return (0);
  }

  if (PMS_Directory_Join(buffer, size, g_szResourceDir, type_filenames[type]) == 0) {
    return (0);
  }

  *len = strlen(buffer);
  return (1);
}

/**
 * \brief Set the name of the directory where the PCL 5 permanent resources are
 * held.
 *
 * If the directory exists or can be created then the function returns 1.  If
 * the directory does not exist and can not be created then it returns 0.
 */
int PMS_PCL5ResourceSetDirectory(char *szResourceDir)
{
  size_t size;

  if (szResourceDir == NULL) {
    return (0);
  }

  size = strlen(szResourceDir);
  if (size > FILENAME_MAX) {
    return (0);
  }

  /* Create directory for PCL permanent resources if it does not exist */
  if (PMS_Directory_Exists(szResourceDir) == 0 &&
      PMS_Directory_Create(szResourceDir) == 0) {
    return (0);
  }

  /* Remember directory to use for PCL permanent resources */
  memcpy(g_szResourceDir, szResourceDir, size);
  return (1);
}

/**
 * \brief Remove all existing PCL 5 resource files.
 *
 * This allows removal of any pre-existing PCL 5 resource files if the host
 * system does not automatically clear them on system restart.
 */
int PMS_PCL5ResourceClear(void)
{
  static int resource_types[4] = {
    PMS_PCL5ResourceFont,
    PMS_PCL5ResourceSymbolSet,
    PMS_PCL5ResourceUDP,
    PMS_PCL5ResourceMacro
  };
  char filename[FILENAME_MAX];
  size_t len;
  int i;

  for (i = 0; i < 4; i++) {
    if (resource_filename(resource_types[i], filename, FILENAME_MAX, &len) != 1) {
      return (0);
    }
    remove(filename);
  }

  return (1);
}

/**
 * \brief Return a open file based on the PCL 5 resource type and operation.
 *
 * If a resource type file is missing when being loaded then an empty file is
 * created for it first.
 */
static int open_resource_file(int type, int operation, FILE **file)
{
  char filename[FILENAME_MAX];
  size_t len;

  *file = NULL;

  if (resource_filename(type, filename, FILENAME_MAX, &len) != 1) {
    return (0);
  }

  switch (operation) {
  case PMS_PCL5ResourceLoad:
    *file = fopen(filename, "rb");
    if (*file == NULL) {
      *file = fopen(filename, "wb");
      if (*file != NULL) {
        *file = freopen(filename, "rb", *file);
      }
    }
    break;

  case PMS_PCL5ResourceStore:
    *file = fopen(filename, "wb");
    break;
  }

  return (*file != NULL ? 1 : 0);
}


/**
 * \brief Return a PCL resource handle ready to transfer resource data.
 */
int PMS_PCL5ResourceStart(int type, int operation, struct PCL_RESOURCE_HANDLE **handle)
{
  struct PCL_RESOURCE_HANDLE *resource;

  *handle = NULL;

  resource = OSMalloc(sizeof(struct PCL_RESOURCE_HANDLE), PMS_MemoryPoolJob);
  if (resource == NULL) {
    return (0);
  }

  if (open_resource_file(type, operation, &resource->resource_file) != 1) {
    OSFree(resource, PMS_MemoryPoolJob);
    return (0);
  }
  resource->operation = operation;
  resource->last_error = PMS_PCL5ResourceOk;

  *handle = resource;
  return (1);
}


/**
 * \brief Finish using a PCL resource handle for transferring resource data.
 */
int PMS_PCL5ResourceFinish(struct PCL_RESOURCE_HANDLE **handle)
{
  struct PCL_RESOURCE_HANDLE *resource;

  if (handle == NULL) {
    return (1);
  }

  resource = *handle;
  PMS_ASSERT(resource->resource_file != NULL,
             ("PCL resource file is not open."));
  fclose(resource->resource_file);
  OSFree(resource, PMS_MemoryPoolJob);
  *handle = NULL;
  return (1);
}


/**
 * \brief Return the last error recorded by the PCL 5 resource functions.
 */
int PMS_PCL5ResourceError(struct PCL_RESOURCE_HANDLE *handle, int *error)
{
  if (handle == NULL || error == NULL) {
    return (0);
  }

  *error = handle->last_error;
  return (1);
}


/**
 * \brief Transfer PCL 5 resource data using the resource handle.
 */
int PMS_PCL5ResourceTransfer(struct PCL_RESOURCE_HANDLE *handle, unsigned char *buffer, int size, int *bytes)
{
  if (handle == NULL) {
    return (0);
  }

  if (buffer == NULL || size <= 0 || bytes == NULL) {
    handle->last_error = PMS_PCL5ResourceFail;
    return (0);
  }

  switch (handle->operation) {
  case PMS_PCL5ResourceLoad:
    *bytes = (int)fread(buffer, sizeof(unsigned char), size, handle->resource_file);
    break;

  case PMS_PCL5ResourceStore:
    *bytes = (int)fwrite(buffer, sizeof(unsigned char), size, handle->resource_file);
    break;

  default:
    *bytes = 0;
    break;
  }

  if (*bytes < size && ferror(handle->resource_file) != 0) {
    handle->last_error = PMS_PCL5ResourceFail;
    return (0);
  }

  handle->last_error = PMS_PCL5ResourceOk;
  return (1);
}

/* EOF */
