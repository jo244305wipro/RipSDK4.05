/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 *
 * $ Repository Name: EBDSDK_P_HWA1/pms/src/src:oil_hwa1.c $
 *
 */
/*! \file
 *  \ingroup OIL
 *  \brief Implementation of the OIL support functions.
 *
 * This file contains the definitions of the functions which initialize and inactivate the OIL
 * and/or individual jobs, as well as various other support functions, including functions
 * for registering resources and RIP modules with the RIP.
 *
 */
#include "oil_interface_oil2pms.h"
#include <string.h>
#include "skinkit.h"
#include "pgbdev.h"
#include "mem.h"
#include "pms_export.h"
#include "oil_platform.h"
#include "oil.h"
#include "oil_interface_skin2oil.h"
#include "oil_job_handler.h"
#include "oil_ebddev.h"
#include "oil_page_handler.h"
#include "oil_malloc.h"
#include "tec_api.h"
#include "oil_tec.h"

/* extern variables */
extern OIL_TyConfigurableFeatures g_ConfigurableFeatures;
extern OIL_TyJob *g_pstCurrentJob;
extern OIL_TyPage *g_pstCurrentPage;

static uint32 TecData[2][5];
static uint32 *pTecData = &TecData[0][0];
static tec_api_20140912 OIL_TEC_API = {OIL_TEC_SetLevel, &pTecData, 0} ;
static int pagenum;


int *g_pPMSCriteria[2];
static int DataLength;

tec_result RIPCALL OIL_TEC_SetLevel(size_t value)
{

  PMS_SetTECLevel(value);
  return 1;
}

void *OIL_TEC_Interface(unsigned int *plength, const unsigned int *Data, const unsigned int Length )
{
  g_pPMSCriteria[0] = (int *)Data;
  g_pPMSCriteria[1] = (int *)Data + Length;
  DataLength = Length;
  /* initialise the TEC Data table */
  OIL_TEC_API.tec_values = Length/2 ;
  *plength = sizeof(OIL_TEC_API);
  return (&OIL_TEC_API);
}

void OIL_SetTECData(int **TecCriteria)
{
 int i;
 int *pData = g_pPMSCriteria[0];
  /* Update the TEC Data table from the Job setup data */
  if(TecCriteria == NULL)
  {
    OIL_TEC_API.tec_criteria = NULL;
    PMS_SetTECLevel(TECVALUENORMAL);
  }
  else
  {
    for (i = 0; i < DataLength; i++)
    {
        TecData [0][i] =  *(pData + i);
    }
    OIL_TEC_API.tec_criteria = &pTecData;
  }
 
}

