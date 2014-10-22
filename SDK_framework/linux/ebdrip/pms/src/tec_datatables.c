/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 *
 * $ Repository Name: EBDSDK_P_HWA1/pms/src/hwa1_gamma.c $
 * 
 */

/*
 * .\tec_datatables.c: file system definition file for for TEC vale selection and settings.
 *
 * This file was generated from data defined by Rift in their specification
 *
 */
#include "pms_export.h"



static unsigned int TECCriteria600 [2][5] = {{ 0, TECVALUETWO, TECVALUETWO, TECVALUENORMAL, TECVALUETWO },
                                      { 5000, TECVALUEONE, TECVALUEONE, TECVALUENORMAL, TECVALUEONE }};

static unsigned int TECCriteria1200 [2][5] = {{ 0, TECVALUENORMAL, TECVALUENORMAL, TECVALUENORMAL, TECVALUENORMAL },
                                      { 5000, TECVALUENORMAL, TECVALUENORMAL, TECVALUENORMAL, TECVALUENORMAL }};

const unsigned int *g_tec_basic_data[2]  = { &TECCriteria600[0][0], &TECCriteria1200[0][0] };

const unsigned int g_tec_basic_data_len[2] = {(sizeof(TECCriteria600)/sizeof(TECCriteria600[0][0])),
                                                (sizeof(TECCriteria1200)/sizeof(TECCriteria1200[0][0]))};



