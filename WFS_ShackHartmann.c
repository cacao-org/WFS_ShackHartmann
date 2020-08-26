/**
 * @file    WFS_ShackHartmann.c
 * @brief   process Shack-Hartmann type wavefront sensors
 *
 * Compute centroids / slopes from raw SHWFS frames
 *
 *  Files :
 * 
 * - SHWFS_process.c  : compute centroids/slopes
 * - SHWFS_process.h  : function prototypes
 * - CMakeLists.txt         : cmake input file for module
 */



/* ================================================================== */
/* ================================================================== */
/*            MODULE INFO                                             */
/* ================================================================== */
/* ================================================================== */

// module default short name
// all CLI calls to this module functions will be <shortname>.<funcname>
// if set to "", then calls use <funcname>
#define MODULE_SHORTNAME_DEFAULT "shwfs"

// Module short description
#define MODULE_DESCRIPTION       "Shack-Hartmann type wavefront sensors"


// Application to which module belongs
#define MODULE_APPLICATION       "cacao"





/* ================================================================== */
/* ================================================================== */
/*            DEPENDANCIES                                            */
/* ================================================================== */
/* ================================================================== */


#define _GNU_SOURCE
#include "CommandLineInterface/CLIcore.h"



//
// Forward declarations are required to connect CLI calls to functions
// If functions are in separate .c files, include here the corresponding .h files
//
#include "SHWFS_process.h"




/* ================================================================== */
/* ================================================================== */
/*           MACROS, DEFINES                                          */
/* ================================================================== */
/* ================================================================== */




/* ================================================================== */
/* ================================================================== */
/*            GLOBAL VARIABLES                                        */
/* ================================================================== */
/* ================================================================== */





/* ================================================================== */
/* ================================================================== */
/*            INITIALIZE LIBRARY                                      */
/* ================================================================== */
/* ================================================================== */

// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//
INIT_MODULE_LIB(WFS_ShackHartmann)


/* ================================================================== */
/* ================================================================== */
/*  MODULE CLI INITIALIZATION                                             */
/* ================================================================== */
/* ================================================================== */

/**
 * @brief Initialize module CLI
 *
 * CLI entries are registered: CLI call names are connected to CLI functions.\n
 * Any other initialization is performed\n
 *
 */
static errno_t init_module_CLI()
{


	SHWFS_process_addCLIcmd();


    // optional: add atexit functions here

    return RETURN_SUCCESS;
}







/* ================================================================== */
/* ================================================================== */
/*  FUNCTIONS                                                         */
/* ================================================================== */
/* ================================================================== */

// Functions could be added here, or (better) in separate .c files

// Each .c file should have a corresponding .h file including
// function prototypes







