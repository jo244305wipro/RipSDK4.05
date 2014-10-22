/* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 *
 * $ Repository Name: EBDSDK_P_HWA1/pms/src/oil_hwa1.h $
 *
 */

/*! \file
 *  \ingroup OIL
 *  \brief This header file declares the HWA oil functions to be accessed by PMS.
 *
 */


#ifndef _OIL_TEC_H_
#define _OIL_TEC_H_

tec_result RIPCALL OIL_TEC_SetLevel(size_t value);
void OIL_SetTECData(int **TecCriteria);
extern int *g_pPMSCriteria[2];


#endif /* _OIL_TEC_H_ */
