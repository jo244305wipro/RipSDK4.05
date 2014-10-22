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


#ifndef _OIL_HWA_H_
#define _OIL_HWA_H_

hwa_result RIPCALL OIL_HWA1_GetBuffer(sw_tl_ref page, uint8 ** pbuffer, size_t * plength);
hwa_result RIPCALL OIL_HWA1_SubmitBuffer(sw_tl_ref page, int32 fromband, int32 toband,
                                            uint8 * orderlist, size_t orderlen, uint8 * assets, size_t assetlen,
                                                                                                    HqBool complete);
hwa_result RIPCALL OIL_HWA1_DiscardBuffer(sw_tl_ref page, uint8 * buffer) ;
hwa_result RIPCALL OIL_HWA1_GetBands(sw_tl_ref page, uint8 * buffer, int32 bandid,
                                       uint8 ** pbandC, uint8 ** pbandM, uint8 ** pbandY, uint8 ** pbandK);
hwa_result RIPCALL OIL_HWA1_DiscardBands(sw_tl_ref page, uint8 * buffer, int32 bandid,
                                      uint8 * bandC, uint8 * bandM, uint8 * bandY, uint8 * bandK);
hwa_result RIPCALL OIL_HWA1_GetOptions(sw_tl_ref page, hwa_options *values);

void RIPCALL OIL_HWA1_RasterDescription(RASTER_DESTINATION * pRasterDestination, RasterDescription *pRasterDescription);

void OIL_HWA1_PageComplete(void);


#endif /* _OIL_HWA_H_ */
