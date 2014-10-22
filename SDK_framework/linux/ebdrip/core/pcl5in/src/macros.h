/** \file
 * \ingroup pcl5
 *
 * $HopeName: COREpcl_pcl5!src:macros.h(EBDSDK_P.1) $
 *
 * Copyright (C) 2007-2014 Global Graphics Software Ltd. All rights
 * reserved. Global Graphics Software Ltd. Confidential Information.
 */

#ifndef __MACROS_H__
#define __MACROS_H__

#include "pcl5context_private.h"
#include "fileio.h"

#define MAX_MACRO_NEST_LEVEL 3


/* Track whether the last macro id was set as a numeric or a string */
#define MACRO_SET_NUMERIC (0)
#define MACRO_SET_STRING  (1)


/* Be sure to update default_macro_info() if you change this
   structure. */
typedef struct MacroInfo {
  int last_set;

  /* Explicitly required */
  pcl5_resource_numeric_id macro_numeric_id ;
  pcl5_resource_string_id macro_string_id ;

  pcl5_resource_numeric_id overlay_macro_numeric_id ;
} MacroInfo;

/* Although I'd prefer to avoid globals I decided to make this test a
   global to avoid a function call per byte in the PCL5/HPGL2
   stream. I did not even want to do an indirection. */

/* Are we recording a macro? Only one macro can ever be recoreded at once. */
extern Bool pcl5_recording_a_macro ;

/* When we are not executing a macro, this global is zero, otherwise
   it is incremented once per macro nest level up to a max of three
   (as per spec). */
extern uint32 pcl5_macro_nest_level ;

/* Macro modes. */
enum {
  PCL5_NOT_EXECUTING_MACRO,
  PCL5_EXECUTING_CALLED_MACRO,
  PCL5_EXECUTING_EXECUTED_MACRO,
  PCL5_EXECUTING_OVERLAY_MACRO,
} ;
/* What macro mode are we in? */
extern int32 pcl5_current_macro_mode ;

/* When a job starts/completes, these functions get called. */
Bool pcl5_macros_init(PCL5Context *pcl5_ctxt) ;
void pcl5_macros_finish(PCL5Context *pcl5_ctxt) ;

/* Sets the current Macro ID to the String ID. */
void set_macro_string_id(PCL5Context *pcl5_ctxt, pcl5_resource_string_id *string_id);
/* Associates the current Macro number ID to the supplied string ID. */
void associate_macro_string_id(PCL5ResourceCaches *caches, pcl5_resource_numeric_id id, pcl5_resource_string_id *string_id);
/* Deletes the macro association named by the current Macro string ID. */
void delete_macro_associated_string_id(PCL5Context *pcl5_ctxt);

/* Macro info commands. */
void default_macro_info(MacroInfo* self);
MacroInfo* get_macro_info(PCL5Context *pcl5_ctxt) ;

/* We need these two exported so that we can handle ESC-E while
   recording a macro. */
/* TODO - replace with abort function */
void remove_macro_definition(PCL5Context *pcl5_ctxt, MacroInfo *macro_info) ;
Bool stop_macro_definition(PCL5Context *pcl5_ctxt, MacroInfo *macro_info) ;

/* Execute a macro. */
Bool execute_macro_definition(PCL5Context *pcl5_ctxt, pcl5_resource_numeric_id id, Bool overlay) ;

/* We need this so that we can cleaup the macro info structure which
   may have allocated strings. */
void cleanup_macroinfo_strings(MacroInfo *macro_info) ;

void macro_free_data(pcl5_macro *macro);

Bool save_macro_info(PCL5Context *pcl5_ctxt, MacroInfo *to_macro_info, MacroInfo *from_macro_info) ;
void restore_macro_info(PCL5Context *pcl5_ctxt, MacroInfo *to_macro_info, MacroInfo *from_macro_info);

#define MACROS_DESTROY_ALL        (TRUE)
#define MACROS_DESTROY_TEMPORARY  (FALSE)
void destroy_macros(PCL5Context *pcl5_ctxt, Bool include_permanent) ;

/* PCL5 macro related operators. */
Bool pcl5op_ampersand_f_Y(PCL5Context *pcl5_ctxt, int32 explicit_sign, PCL5Numeric value) ;
Bool pcl5op_ampersand_f_X(PCL5Context *pcl5_ctxt, int32 explicit_sign, PCL5Numeric value) ;

/* PCL5 macro recording filter. Gets added to the standard filter list
   at RIP boot time. */
Bool pcl5_macro_record_filter_init(void) ;

Bool pcl5_macro_store_permanent(struct PCL_RESOURCE_HANDLE *handle, pcl5_macro *macro);
Bool pcl5_macro_load_permanent(PCL5ResourceCaches *resource_caches, struct PCL_RESOURCE_HANDLE *handle);

/* ============================================================================
* Log stripped */
#endif
