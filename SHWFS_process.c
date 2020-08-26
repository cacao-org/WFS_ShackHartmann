/** @file SHWFS_process.c
 */

#include <math.h>

#include "CommandLineInterface/CLIcore.h"
#include "COREMOD_memory/image_ID.h"
#include "COREMOD_memory/stream_sem.h"
#include "COREMOD_memory/create_image.h"




typedef struct
{
	// lower index pixel coords in input raw image
    uint32_t Xraw;
    uint32_t Yraw;
    
    // precomputed indices for speed
    uint64_t XYraw00;
    uint64_t XYraw01;
    uint64_t XYraw10;
    uint64_t XYraw11;
                
    // output 2D coordinates
    uint32_t Xout;
    uint32_t Yout;

    // precomputed indices for speed
    uint64_t XYout_dx;
    uint64_t XYout_dy;
    
    // signal
    float dx;
    float dy;
    float flux;
    
} SHWFS_QUADCELL_SPOT;


#define MAXNB_SPOT 1000







// ==========================================
// Forward declaration(s)
// ==========================================



errno_t SHWFS_process_FPCONF();

errno_t SHWFS_process_RUN();


errno_t SHWFS_process(
    const char *IDin_name,
    const char *conf_fname
);






// ==========================================
// Command line interface wrapper function(s)
// ==========================================


static errno_t SHWFS_process__cli()
{
    // Try FPS implementation

    // Set data.fpsname, providing default value as first arg, and set data.FPS_CMDCODE value.
    // Default FPS name will be used if CLI process has NOT been named.
    // See code in function_parameter.c for detailed rules.

    function_parameter_getFPSargs_from_CLIfunc("SHWFSprocess");

    if(data.FPS_CMDCODE != 0) { // use FPS implementation
        // set pointers to CONF and RUN functions
        data.FPS_CONFfunc = SHWFS_process_FPCONF;
        data.FPS_RUNfunc  = SHWFS_process_RUN;
        function_parameter_execFPScmd();
        return RETURN_SUCCESS;
    }


    // call non FPS implementation - all parameters specified at function launch
    if(
        CLI_checkarg(1, CLIARG_IMG) +
        CLI_checkarg(2, CLIARG_STR)
        == 0) {
        SHWFS_process(
            data.cmdargtoken[1].val.string,
            data.cmdargtoken[2].val.string
        );

        return RETURN_SUCCESS;
    } else {
        return CLICMD_INVALID_ARG;
    }
}
 



// ==========================================
// Register CLI command(s)
// ==========================================

errno_t SHWFS_process_addCLIcmd()
{

    RegisterCLIcommand(
        "SHWFSprocess",
        __FILE__,
        SHWFS_process__cli,
        "computes slopes/centroids from SHWFS frames",
        "<instream> <conf file>",
        "SHWFSprocess imSHWFS SHconf",
        "SHWFS_process(char *imstream, char *conffile)");

    return RETURN_SUCCESS;
}









/**
 * @brief Manages configuration parameters for SHWFS_process
 *
 *
 */
errno_t SHWFS_process_FPCONF()
{
	// ===========================
    // SETUP FPS
    // ===========================
    FPS_SETUP_INIT(data.FPS_name, data.FPS_CMDCODE); // macro in function_parameter.h
    fps_add_processinfo_entries(&fps); // include real-time settings

    // ==============================================
    // ========= ALLOCATE FPS ENTRIES ===============
    // ==============================================

    FPS_ADDPARAM_STREAM_IN   (stream_inname,  ".rawWFSin", "input raw WFS stream", NULL);
    FPS_ADDPARAM_FILENAME_IN (conffname, ".spotcoords", "SH spot coordinates", NULL);
    FPS_ADDPARAM_STREAM_OUT  (stream_outname, ".outWFS",   "output stream");

	
    long AlgorithmMode_default[4] = { 1, 1, 3, 0 };
    FPS_ADDPARAM_INT64_IN  (
        option_timeavemode,
        ".algomode",
        "Algorithm mode",
        &AlgorithmMode_default);


	// ==============================================
    // ======== START FPS CONF LOOP =================
    // ==============================================
    FPS_CONFLOOP_START  // macro in function_parameter.h


    // ==============================================
    // ======== STOP FPS CONF LOOP ==================
    // ==============================================
    FPS_CONFLOOP_END  // macro in function_parameter.h


    return RETURN_SUCCESS;
}





static int SHWFS_read_spots_coords(SHWFS_QUADCELL_SPOT *spotcoord, char *fname)
{
    int NBspot = 0;

    FILE *fp;

    fp = fopen(fname, "r");
    if(fp == NULL)
    {
        perror("Unable to open file!");
        exit(1);
    }


    int xin, yin, xout, yout;

    char keyw[16];

    int loopOK = 1;
    while(loopOK == 1)
    {
        int ret = fscanf(fp, "%s %d %d %d %d", keyw, &xin, &yin, &xout, &yout);
        if(ret == EOF)
        {
            loopOK = 0;
        }
        else
        {
			
            if( (ret==5) && (strcmp(keyw, "SPOT") == 0))
            {
                printf("Found SPOT %5d %5d   %5d %5d\n", xin, yin, xout, yout);
                spotcoord[NBspot].Xraw = xin;
                spotcoord[NBspot].Yraw = yin;
                spotcoord[NBspot].Xout = xout;
                spotcoord[NBspot].Yout = yout;               
                NBspot++;
            }
        }
    }
	printf("Loaded %d spots\n", NBspot);

    fclose(fp);

    return NBspot;
}





/** @brief Loop process code example
 *
 * ## Purpose
 *
 * This example demonstrates use of processinfo and fps structures.\n
 *
 *
 * All function parameters are held inside the function parameter structure (FPS).\n
 * 
 *
 * ## Details
 *
 */

errno_t SHWFS_process_RUN()
{
    // ==================================================
    // ### Connect to FUNCTION PARAMETER STRUCTURE (FPS)
    // ==================================================
    FPS_CONNECT(data.FPS_name, FPSCONNECT_RUN);


    // ===================================
    // ### GET VALUES FROM FPS
    // ===================================
    // parameters are addressed by their tag name

    // These parameters are read once, before running the loop
    //

    // Algorith mode
    //
    // 0 : undefined
    // 1 : Quad Cell
    // 2 : Photocenter
    //
    int AlgorithmMode = functionparameter_GetParamValue_INT64(&fps, ".algomode");
    (void) AlgorithmMode;

    char rawWFS_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(rawWFS_streamname, functionparameter_GetParamPtr_STRING(&fps,
            ".rawWFSin"), FUNCTION_PARAMETER_STRMAXLEN);

    char outWFS_streamname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(outWFS_streamname, functionparameter_GetParamPtr_STRING(&fps,
            ".outWFS"), FUNCTION_PARAMETER_STRMAXLEN);




    // ===========================
    // ### processinfo support
    // ===========================

    PROCESSINFO *processinfo;

    processinfo = processinfo_setup(
                      data.FPS_name,               // re-use fpsname as processinfo name
                      "process SHWFS to slopes",        // description
                      "startup",      // message on startup
                      __FUNCTION__, __FILE__, __LINE__
                  );

    // OPTIONAL SETTINGS

    // Measure timing
    processinfo->MeasureTiming = 1;




	// Allocate spots
	SHWFS_QUADCELL_SPOT *spotcoord = (SHWFS_QUADCELL_SPOT*) malloc(sizeof(SHWFS_QUADCELL_SPOT)*MAXNB_SPOT);


    // ===========================
    // read spot coordinates
    // ===========================
    char spotcoords_fname[FUNCTION_PARAMETER_STRMAXLEN + 1];
    strncpy(spotcoords_fname, functionparameter_GetParamPtr_STRING(&fps, ".spotcoords"), FUNCTION_PARAMETER_STRMAXLEN);
    
    char msgstring[200];
    sprintf(msgstring, "Loading spot <- %s", spotcoords_fname);
    processinfo_WriteMessage(processinfo, msgstring);

	int NBspot = SHWFS_read_spots_coords(spotcoord, spotcoords_fname);

	// size of output 2D representation
	imageID IDin = image_ID(rawWFS_streamname);
	uint32_t sizeinX = data.image[IDin].md[0].size[0];
	uint32_t sizeinY = data.image[IDin].md[0].size[1];
	uint32_t sizeoutX = 0;
	uint32_t sizeoutY = 0;
	for(int spot=0; spot<NBspot; spot++)
	{
				// geom
				// 10 11
				// 00 01
						
		spotcoord[spot].XYraw00 = spotcoord[spot].Yraw * sizeinX + spotcoord[spot].Xraw;
		spotcoord[spot].XYraw01 = spotcoord[spot].Yraw * sizeinX + spotcoord[spot].Xraw + 1;
		spotcoord[spot].XYraw10 = (spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw;
		spotcoord[spot].XYraw11 = (spotcoord[spot].Yraw + 1) * sizeinX + spotcoord[spot].Xraw + 1;
				
		if ( spotcoord[spot].Xout + 1 > sizeoutX )
		{
			sizeoutX = spotcoord[spot].Xout+1;
		}
		if ( spotcoord[spot].Yout + 1 > sizeoutY )
		{
			sizeoutY = spotcoord[spot].Yout+1;
		}
	}
	for(int spot=0; spot<NBspot; spot++)
	{
		spotcoord[spot].XYout_dx = spotcoord[spot].Yout * (2*sizeoutX) + spotcoord[spot].Xout;
		spotcoord[spot].XYout_dy = spotcoord[spot].Yout * (2*sizeoutX) + spotcoord[spot].Xout + sizeoutX;
	}
	printf("Output 2D representation: %d x %d\n", sizeoutX, sizeoutY);
	
	
	
	// create output arrays
	char outname[200];
	uint32_t *imsizearray = (uint32_t*) malloc(sizeof(uint32_t)*2);
	
	// centroids
	sprintf(outname, "%s_cent", outWFS_streamname);	
	imsizearray[0] = sizeoutX * 2;
	imsizearray[1] = sizeoutY;
	create_image_ID(outname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10);

	// 2D input pixels
	sprintf(outname, "%s_2Din", outWFS_streamname);	
	imsizearray[0] = sizeoutX * 2;
	imsizearray[1] = sizeoutY * 2;
	create_image_ID(outname, 2, imsizearray, _DATATYPE_FLOAT, 1, 10);	

	free(imsizearray);
	
	
	

    // apply relevant FPS entries to PROCINFO
    // see macro code for details
    fps_to_processinfo(&fps, processinfo);


    int loopOK = 1;





    // Specify input stream trigger
    IDin = image_ID(rawWFS_streamname);
    processinfo_waitoninputstream_init(processinfo, IDin,
        PROCESSINFO_TRIGGERMODE_SEMAPHORE, 1);
        
    printf("IDin = %ld\n", (long) IDin);

    // Identifiers for output streams
    sprintf(outname, "%s_cent", outWFS_streamname);
    imageID IDout = image_ID(outname);

    sprintf(outname, "%s_2Din", outWFS_streamname);
    imageID IDout2Din = image_ID(outname);


    // ===========================
    // ### Start loop
    // ===========================

    // Notify processinfo that we are entering loop
    processinfo_loopstart(processinfo);

    while(loopOK==1)
    {
        loopOK = processinfo_loopstep(processinfo);
        
        processinfo_waitoninputstream(processinfo);

        processinfo_exec_start(processinfo);
        if(processinfo_compute_status(processinfo)==1)
        {
            //
            // computation ....
            //
            
            for(int spot=0; spot<NBspot; spot++)
            {
				float dx = 0;
				float dy = 0;
				float flux = 0;
				
				// geom
				// 10 11
				// 00 01
				
				float f00 = data.image[IDin].array.F[spotcoord[spot].XYraw00];
				float f01 = data.image[IDin].array.F[spotcoord[spot].XYraw01];
				float f10 = data.image[IDin].array.F[spotcoord[spot].XYraw10];
				float f11 = data.image[IDin].array.F[spotcoord[spot].XYraw11];
				
				flux = f00 + f01 + f10 + f11;
				dx = (f01 + f11) - (f00 + f10);
				dx /= flux;
				dy = (f10 + f11) - (f00 + f01);
				dy /= flux;				
				 
				spotcoord[spot].dx = dx;
				spotcoord[spot].dy = dy;
				spotcoord[spot].flux = flux;
			}
			

            data.image[IDout].md[0].write = 1;
            data.image[IDout2Din].md[0].write = 1;
            // write into output images
            
            for(int spot=0; spot<NBspot; spot++)
            {
				uint32_t spotX = spotcoord[spot].Xout;
				uint32_t spotY = spotcoord[spot].Yout;
				
				data.image[IDout2Din].array.F[2*spotY*(2*sizeoutX) + 2*spotX] = data.image[IDin].array.F[spotcoord[spot].XYraw00];
				data.image[IDout2Din].array.F[2*spotY*(2*sizeoutX) + 2*spotX+1] = data.image[IDin].array.F[spotcoord[spot].XYraw01];
				data.image[IDout2Din].array.F[(2*spotY+1)*(2*sizeoutX) + 2*spotX] = data.image[IDin].array.F[spotcoord[spot].XYraw10];
				data.image[IDout2Din].array.F[(2*spotY+1)*(2*sizeoutX) + 2*spotX+1] = data.image[IDin].array.F[spotcoord[spot].XYraw11];
				
				data.image[IDout].array.F[spotcoord[spot].XYout_dx] = spotcoord[spot].dx;
				data.image[IDout].array.F[spotcoord[spot].XYout_dy] = spotcoord[spot].dy;				
			}

            // Post semaphore(s) and counter(s), notify system that outputs have been written
            processinfo_update_output_stream(processinfo, IDout);
            processinfo_update_output_stream(processinfo, IDout2Din);
        }

        // process signals, increment loop counter
        processinfo_exec_end(processinfo);

        // OPTIONAL: MESSAGE WHILE LOOP RUNNING
        char msgstring[200];
        sprintf(msgstring, "loop running ID %ld", (long) IDin );
        processinfo_WriteMessage(processinfo, msgstring);
    }


    // ==================================
    // ### ENDING LOOP
    // ==================================

    processinfo_cleanExit(processinfo);

	free(spotcoord);


    return RETURN_SUCCESS;
}





errno_t SHWFS_process(
    const char *IDin_name,
    const char *conf_fname
)
{
    long pindex = (long)
                  getpid();  // index used to differentiate multiple calls to function
    // if we don't have anything more informative, we use PID

    FUNCTION_PARAMETER_STRUCT fps;

    // create FPS
    sprintf(data.FPS_name, "shwfsproc-%06ld", pindex);
    data.FPS_CMDCODE = FPSCMDCODE_FPSINIT;
    SHWFS_process_FPCONF();

    function_parameter_struct_connect(data.FPS_name, &fps, FPSCONNECT_SIMPLE);

	functionparameter_SetParamValue_STRING(&fps, ".rawWFSin", IDin_name);
	functionparameter_SetParamValue_STRING(&fps, ".spotcoords", conf_fname);
	functionparameter_SetParamValue_STRING(&fps, ".outWFS", "outWFS");
    functionparameter_SetParamValue_INT64(&fps, ".algomode", 1);

    function_parameter_struct_disconnect(&fps);

    SHWFS_process_RUN();

    return RETURN_SUCCESS;
}
