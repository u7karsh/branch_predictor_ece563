/*H**********************************************************************
* FILENAME    :       bp.h
* DESCRIPTION :       Contains structures and prototypes for generic
*                     branch predictor simulator
* NOTES       :       -NA-
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    20 Oct 17
*
* CHANGES :
*
*H***********************************************************************/


#ifndef _BP_H
#define _BP_H

#include "all.h"

// Mask for 32 bit address
#define   MSB_ONE_32_BIT    0x80000000
#define   BP_LOWER_RSVD_CNT 2

// Pointer translations
typedef  struct  _bpT                 *bpPT;
typedef  struct  _bpPathT             *bpPathPT;

// Enum to hold direction like read/write
typedef enum {
   BP_PATH_NOT_TAKEN                         = 0,
   BP_PATH_TAKEN                             = 1,
}bpPathT;

typedef enum{
   BP_TYPE_BIMODAL                           = 0,
   BP_TYPE_GSHARE                            = 1,
   BP_TYPE_HYBRID                            = 2,
}bpTypeT;

// Branch predictor structure.
typedef struct _bpT{
   /*
    * Configutration params
    */
   // Placeholder for name
   char                  name[128];

   // Number of bits used to represent prediction table
   int                   mBimodal;
   int                   mGshare;

   int                   n;
   int                   k;
   bpTypeT               type;
   int                   gShareShift;
   int                   nMsbSetMask;
   // Mask to calculate index from address
   int                   mBimodalMask;
   int                   mGshareMask;

   int                   nMask;
   int                   kMask;

   int                   sizePredictionTableBimodal;
   int                   sizePredictionTableGshare;

   int                   *predictionTableBimodal;
   int                   *predictionTableGshare;

   // Chooser table for hybrid predictor
   int                   sizeChooserTable;
   int                   *chooserTable;
   // Register for GShare predictor
   int                   globalBrHistoryReg;

   // Metrics
   int                   predictions;
   int                   misPredictions;

}bpT;

// Function prototypes. This part can be automated
// lets keep hardcoded as of now
bpPT  branchPredictorInit(   
      char*              name, 
      int                mBimodal,
      int                mGshare,
      int                n,
      int                k,
      bpTypeT            type
      );
void bpProcess( bpPT bpP, int address, bpPathT actual );
bpPathT bpPredict( bpPT bpP, int index, bpTypeT type );
void bpUpdatePredictionTable( bpPT bpP, int index, bpPathT actual, bpTypeT takenType );
int bpGetChooserIndex( bpPT bpP, int address );
bpTypeT bpHybridGetType( bpPT bpP, int index );
int bpGetIndex( bpPT bpP, int address, bpTypeT type );
void bpGetMetrics( bpPT bpP, int* predictions, int* misPredictions, double* misPredictionRate );
void bpUpdateChooserTable( bpPT bpP, int chooserIndex, bpTypeT type, bpPathT pred1, bpPathT pred2, bpPathT actual );

void bpPrintPredictionTableBimodal( bpPT bpP );
void bpPrintPredictionTableGshare( bpPT bpP );
void bpPrintChooserTable( bpPT bpP );
#endif
