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

#ifndef _PMS_PCL5PERMRES_H_
#define _PMS_PCL5PERMRES_H_ (1)

struct PCL_RESOURCE_HANDLE;

int PMS_PCL5ResourceSetDirectory(char *szResourceDir);
int PMS_PCL5ResourceClear(void);
int PMS_PCL5ResourceStart(int type, int operation, struct PCL_RESOURCE_HANDLE **handle);
int PMS_PCL5ResourceFinish(struct PCL_RESOURCE_HANDLE **handle);
int PMS_PCL5ResourceError(struct PCL_RESOURCE_HANDLE *handle, int *error);
int PMS_PCL5ResourceTransfer(struct PCL_RESOURCE_HANDLE *handle, unsigned char *buffer, int size, int *bytes);

#endif /* !_PMS_PCL5PERMRES_H_ */
