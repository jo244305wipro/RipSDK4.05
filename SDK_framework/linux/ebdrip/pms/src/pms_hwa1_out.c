/* Copyright (C) 2005-2012 Global Graphics Software Ltd. All rights reserved.
 *
 * This example is provided on an "as is" basis and without
 * warranty of any kind. Global Graphics Software Ltd. does not
 * warrant or make any representations regarding the use or results
 * of use of this example.
 *
 * $ Repository Name: EBDSDK_P_HWA1/pms/src/pms_hwa_out.c $
 *
 */
/*! \file
 *  \ingroup PMS
 *  \brief PMS output functions fro HWA 1 (GIPA2).
 */
#include "pms.h"
#include "pms_malloc.h"
#include "pms_pdf_out.h"
#include "zlib.h"
#include <string.h>


static FILE * hFileOut;

/*
Header information, Order List data and Working data. 
Header File:		  OLHeadj_1_p01_w4864_h6814_d1_c01
Order List file:	OLj_1_p01_w4864_h6814_d1_c01
Work file:		    OLWorkj_1_p01_w4864_h6814_d1_c01

  where
            j: Job number
            p: Page number
                Note: Add “0” at the top for one digit number only.
            w: Band width in pixel unit
            h: Imageable page height in pixel unit
            d: Pixel depth (1, 2, 4)
            c: OL serial number in one page
                Note: Add “0” at the top for one digit number only.
*/

/* 
HWA Header File Format for FPGA output 
1	CMYKFLAG
  It should be same value as PLANEFLG of GIPA2_BAND_INIT.	This value depend on color mode:
  1.CMYK Output ?0000000f
  2.Black&White   ?00000001
2	Band Start Address for Black plane
  It should be same address of GIPA2_BAND_INIT.
  It shouldn’t overlap other plane address.	This is fixed value. 
  03000000
3	Band Start Address for Cyan plane
  It should be same address of GIPA2_BAND_INIT.
  It shouldn’t overlap other plane address.	This is fixed value.
  04000000
4	Band Start Address for Magenta plane
  It should be same address of GIPA2_BAND_INIT.
  It shouldn’t overlap other plane address.	This is fixed value.
  01000000
5	Band Start Address for Yellow plane
  It should be same address of GIPA2_BAND_INIT.
  It shouldn’t overlap other plane address.	This is fixed value.
  02000000
6	Byte value as Width of Processed Image Band
  It should be same value of GIPA2_BAND_INIT.	This value depend on the band width of each page and it should be byte unit and HEX expression.
  If the width of band is 6400 pixel, it should be 00000320.
7	Bandheight of Processed Image Band (# of pixel)
  It should be same value of GIPA2_BAND_INIT.	This value depends on the height of each page and it should be pixel unit and HEX expression.
  If the height of band is 256 pixel, it should be 00000100.
8	SRAM_CLR_FLG
  This flag performs DMA of initialization of Band for internal SRAM.
  Refer to BAND_SRAM_CLR_FLG.
  Processing Procedures in case I-Mem is used:
    00000000
  Processing Procedures in case I-Mem is not used:
    1)	In the case of clear of internal SRAM band is performed at first:   00000001
    2)	In the case of Clear of internal SRAM band is not performed: 00000000
9	SRAM_DMA_FLG 
  This flag performs DMA of re-drawing of Band for internal SRAM.
  Refer to BAND_SRAM_DMA_FLG.	Processing Procedures in case I-Mem is used:
    00000000
  Processing Procedures in case I-Mem is not used:
    1)	In the case of re-drawing to internal SRAM band is performed at first:   00000000
    2)	In the case of re-drawing DMA of internal SRAM band is not performed: 00000001
10	ICOMAD address
  Start address of I-Mem command is specified to ICOMAD. Or Offset start address of internal SRAM command is specified to ICOMAD.	:
  Processing Procedures in case I-Mem is used
    00000000
  Processing Procedures in case I-Mem is not used:
    SRAM_BAND_HIGH*BAND_WIDTH
11	VCOMAD address
  V-Mem Command Start address	Processing
  Procedures in case I-Mem is used:
    00000000
  Processing Procedures in case I-Mem is not used:
    00800000
12	Commands occupied size by bytes. 
  OL size specify by byte unit and Hex expression.
13	IDATAAD address
  IMEM Data address.
  This is fixed value.
  00100000
14	VDATAAD address
  VMEM Data address.
  This value should be specified each command’s data address in Order List.	This is fixed value.
  00100000
15	Byte count of DATA size
  This is DATA size by byte unit and it should be Hex expression.

The following are only used for a non I-Mem configuration
_________________________________________________________
16	SBANDADDRSS address
  The address of BAND on SRAM 
  It shoud be 8 bytes alignment.
    00000000
17	SBANDHIGH address
  It should be 8 line alignment.
    This is SRAM_BAND_HIGH by pixel and it should be Hex expression. For example 40 and so on.
18	SBANDWORDNUM address
    SRAM_BAND_HIGH*BAND_WIDTH
_________________________________________________________
*/

/* Define this to ensure correct endianness */
/* This is a temporary conditional that if required in the final system will be replaced
    by the noraml platform dependant method using PMS_LOWBYTEFIRST */
#define HWA_ENDIANNESS

/**
 * \brief Open HWA data stream output.
 *
 * Output can be sent to the backchannel or direct to disk.
 *
 * \param pszFilename Filename (including path) of file to write, if file output, otherwise ignored.
 * \return TRUE if successful, FALSE otherwise.
 */
static int HWA_OpenOutput(const char *pszFilename)
{
  hFileOut = fopen(pszFilename, "wb");
  return (hFileOut != NULL);
}

/**
 * \brief Close PDF data stream output.
 *
 * Close the PDF output file if writing direct to file.
 * Free raster buffer and compression buffer used.
 */
static void HWA_CloseOutput()
{

  if(hFileOut)
  {
    fclose(hFileOut);
    hFileOut = NULL;
  }
}

/**
 * \brief Write PDF data stream output.
 *
 * Write direct to disk or send to back channel.
 *
 * \param pBuffer Pointer to data to output.
 * \param nLength Number of bytes to output.
 * \return Number of bytes written.
 */
static int HWA_WriteOutput(char *pBuffer, int nLength)
{
  int nWritten = 0;

  nWritten = (int)fwrite(pBuffer, 1, nLength, hFileOut);
  return nWritten;
}

static int HWA_WriteOutputPlus(unsigned char *pBuffer, int nLength, char * lf)
{
  int nWritten = 0;
  char LocalBuf[16];
#ifdef HWA_ENDIANNESS
  for (; nLength > 0 ; nLength -= 4)
  {
    int bytes = sprintf(LocalBuf, "%02x%02x%02x%02x%s",
                        pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], lf) ;
    pBuffer += 4 ;
    nWritten += HWA_WriteOutput(LocalBuf, bytes);
  }
#else
  int *pBuf = (int *)pBuffer;
  UNUSED_PARAM(char *, lf);
  
  for(; pBuf < (int *)(pBuffer + nLength); pBuf++)
  {
    sprintf(LocalBuf, "%8.8x \n",*pBuf);
    nWritten += (int)fwrite(LocalBuf, 1, 10, hFileOut);
  }
#endif
  return nWritten;
}


/**
 * \brief Write PDF data stream header.
 *
 * Output the PDF file header.
 *
 * \param ptPMSPage Pointer to PMS page structure that contains the complete page.
 * \return PMS_ePDF_Errors error code.
 */
static int HWA_WriteHeaderFile(const char *HeaderFileName,
                               unsigned char * orderlist, size_t orderlen,
                               unsigned char * assets, size_t assetlen,
                               int clear)
{
  PMS_ePDF_Errors eResult = PDF_NoError;
  int length ;
  char buffer[200];
  char *pbuf = &buffer[0];
  unsigned int vcomad = PMS_HWA1_FIXED_VCOMAD;
  unsigned int vdataad = PMS_HWA1_FIXED_VDATAAD ;
#ifndef HWA_USE_IMEM
  UNUSED_PARAM(int, clear);
#endif
  UNUSED_PARAM(unsigned char *, assets);

  /* Fixed VDATAAD is too limiting on size of orderlist, so we use
   * HWA_AUTO_ADDRESS to position the assets immediately after the orderlist,
   * rounded up to a multiple of 16 bytes. Do the same calculation here. */
  vdataad = vcomad + ((orderlen + 0xFFF) &~0xFFF) ;

  /* create the header file data */
  /* colour setting */
  if(g_pstCurrentPMSPage->eColorMode == PMS_Mono)
    pbuf += sprintf(pbuf, "%s\n","00000001");
  else
    pbuf += sprintf(pbuf, "%s\n","0000000f");
  /*	Band Start Address for Black plane */
  pbuf += sprintf(pbuf, "%s\n","03000000");
  /*Band Start Address for Cyan plane */
  pbuf += sprintf(pbuf, "%s\n","04000000");
  /*Band Start Address for Magenta plane */
  pbuf += sprintf(pbuf, "%s\n","01000000");
  /*Band Start Address for Yellow plane */
  pbuf += sprintf(pbuf, "%s\n","02000000");
  /* Byte value as Width of Processed Image Band (HEX)*/
  pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->nRasterWidthBits/8);
  /*Bandheight of Processed Image Band (# of pixel) (HEX)*/
  /* detect FPGA all-one-band mode */
  if (orderlen > 40 && orderlist[16] == 0x11 /* BAND_INIT */) {
    /* First command is a BAND_INIT, so no 'buffer assets' and all one band. */
    pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->nPageHeightPixels) ;
  } else
    pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->atPlane[0].atBand[0].uBandHeight);
  /*	Start address of I-Mem command is specified to ICOMAD. Or Offset start address of internal SRAM command is specified to ICOMAD */
# ifndef HWA_USE_IMEM
  /*	SRAM_CLR_FLG  This flag performs DMA of initialization of Band for internal SRAM. */
  pbuf += sprintf(pbuf, "%s\n","00000000");
  /*	SRAM_DMA_FLG This flag performs DMA of re-drawing of Band for internal SRAM.*/
  pbuf += sprintf(pbuf, "%s\n","00000000"); 
  /*	Start address of I-Mem command is specified to ICOMAD. Or Offset start address of internal SRAM command is specified to ICOMAD */
  pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->atPlane[0].atBand[0].cbBandSize);
  /*	VCOMAD address V-Mem Command Start address	Processing */
  pbuf += sprintf(pbuf, "%s\n","00000000");
# else
  /*	SRAM_CLR_FLG  This flag performs DMA of initialization of Band for internal SRAM. */
  if(clear)
  {
    /* SRAM_CLR_FLG In the case of clear of internal SRAM band is performed at first:   00000001 */
    pbuf += sprintf(pbuf, "%s\n","00000001");
    /* SRAM_DMA_FLG In the case of re-drawing DMA of internal SRAM band is not performed: 00000001 */
    pbuf += sprintf(pbuf, "%s\n","00000000");
  }
  else
  {
    /* SRAM_CLR_FLG In the case of Clear of internal SRAM band is not performed: 00000000 */
    pbuf += sprintf(pbuf, "%s\n","00000000");
    /* SRAM_DMA_FLG In the case of re-drawing to internal SRAM band is performed at first:   00000000 */
    pbuf += sprintf(pbuf, "%s\n","00000001");
  }
  /*	Start address of I-Mem command is specified to ICOMAD. Or Offset start address of internal SRAM command is specified to ICOMAD */
  pbuf += sprintf(pbuf, "%8.8x\n", vcomad);    
  /*	VCOMAD address V-Mem Command Start address	Processing */
  pbuf += sprintf(pbuf, "%8.8x\n", vcomad);
# endif
    /*	OL size specify by byte unit and Hex expression. */
  pbuf += sprintf(pbuf, "%8.8x\n", orderlen);
  /*	IMEM Data address. This is fixed value. */
  pbuf += sprintf(pbuf, "%8.8x\n", vdataad); 
  /* VMEM Data address. This value should be specified each command’s data address in Order List.	This is fixed value. */
  pbuf += sprintf(pbuf, "%8.8x\n", vdataad); 
  /*	This is DATA size by byte unit and it should be Hex expression */
  pbuf += sprintf(pbuf, "%8.8x\n", assetlen);
# ifndef HWA_USE_IMEM
  /*	SBANDADDRSS address The address of BAND on SRAM   It shoud be 8 bytes alignment. */
  pbuf += sprintf(pbuf, "%s\n","00000000");
  /*	SBANDHIGH address It should be 8 line alignment.*/
  pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->atPlane[0].atBand[0].uBandHeight);
  /* SBANDWORDNUM address SRAM_BAND_HIGH*BAND_WIDTH*/
  pbuf += sprintf(pbuf, "%8.8x\n", g_pstCurrentPMSPage->atPlane[0].atBand[0].cbBandSize);    
# endif
  
  HWA_OpenOutput(HeaderFileName);
  /* write the header file */
  length = pbuf - buffer;
  HWA_WriteOutput(buffer, length);
  HWA_CloseOutput();

  return eResult;
}



/**
 * \brief The one and only function call to the HWA output method.
 *
 * \param ptPMSPage Pointer to PMS page structure that contains the complete page.
 * \return PMS_ePDF_Errors error code.
 */
int HWA_FPGA_FileHandler(unsigned char *orderlist, size_t orderlen,
                                      unsigned char *assets, size_t assetlen,
                                                        int complete, int index)
{
  char FileName[256];
  char FilePath[256];
  char FileString[64];
  char *pStr;
  PMS_ePDF_Errors eResult = PDF_NoError;

  if(g_tSystemInfo.szOutputPath[0])
  {
    memcpy(FilePath,g_tSystemInfo.szOutputPath,strlen(g_tSystemInfo.szOutputPath));
    pStr=strrchr(g_pstCurrentPMSPage->szJobName,'/');
    strcat(FileName,"/");
    if(pStr)
    {
      strcat(FilePath,pStr+1);
    }
    else
    {
      strcat(FilePath,g_pstCurrentPMSPage->szJobName);
    }
  }
  else
  {
    strncpy(FilePath, g_pstCurrentPMSPage->szJobName, sizeof(FilePath)-1);
  }

  pStr = FilePath + strlen(FilePath);
  while((pStr > FilePath) && (*(pStr-1)!='/') && (*(pStr-1)!=':')) pStr--;
  /* create file name for header file OLHeadj_1_p01_w4864_h6814_d1_c01 */
  sprintf(FileString, "j_%d_p%2.2d_w%d_h%d_d%d_c%2.2d", g_pstCurrentPMSPage->JobId, g_pstCurrentPMSPage->PageId,
                                                        g_pstCurrentPMSPage->nPageWidthPixels, g_pstCurrentPMSPage->nPageHeightPixels,
                                                        g_pstCurrentPMSPage->uOutputDepth, index);
  *pStr = 0;
  sprintf(FileName, "%sOLHead%s", FilePath, FileString);   
  HWA_WriteHeaderFile(FileName, orderlist, orderlen, assets, assetlen, complete);
  /* create file name for orderlist  OLj_1_p01_w4864_h6814_d1_c01 */
  sprintf(FileName, "%sOL%s", FilePath, FileString);  
  HWA_OpenOutput(FileName);
  /* write the orderlist file */
  HWA_WriteOutputPlus(orderlist, orderlen, " \n");
  HWA_CloseOutput();
  /* create file name for assets OLWorkj_1_p01_w4864_h6814_d1_c01 */
  sprintf(FileName, "%sOLWork%s", FilePath, FileString);  
  HWA_OpenOutput(FileName);
  /* write the assetlist file */
  HWA_WriteOutputPlus(assets, assetlen, " \n");
  HWA_CloseOutput();
 
  return eResult;
}

