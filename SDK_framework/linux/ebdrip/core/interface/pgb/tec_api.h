/** \file
* \ingroup interface
*
* $HopeName: COREinterface_pgb!tec_api.h(EBDSDK_P.1) $
*
* Copyright (C) 2014 Global Graphics Software Ltd. All rights reserved.
* Global Graphics Software Ltd. Confidential Information.
*
* \brief
* This header file provides details of the TEC API
*/


#ifndef __TEC_API_H__
#define __TEC_API_H__

#include "rdrapi.h"
#include "timelineapi.h"

/** \brief TEC_API for providing TEC criteria data and TEC function to set level */

/** \brief TEC API return values */
typedef int tec_result ;  /* Return values from the TEC API */

enum {
  TEC_SUCCESS = 0,    /**< Successful call */
  TEC_ERROR,          /**< Some unknown error occurred */
};

/** \brief The TEC_API 201400912 structure.

    This is registered as RDR_API_TEC, defined in apis.h

 */
typedef struct {
  /** \brief  Pass the TEC Value to the outside world.

      \param value    TEC level to be set for the Fuser Control System.

      \return         TEC_SUCCESS value accepted.

                      TEC_ERROR provision for failure returns

      This is called by the DL TEC monitoring function when anew TEC level has been detected. 
  
  */
  tec_result (RIPCALL *set_level)( size_t value) ;

   /** \brief  this pointer points at the relevant TEC criteria table based on the job parameters
   The table is updated at the start of each job, the data collected and applied if necessary
   */
  uint32 **tec_criteria ;        /**< Pointer to TEC Criteria data. */
  uint32 tec_values ;                /**< number of values in each data array. */

} tec_api_20140912 ;

#endif /* __TEC_API_H__ */

