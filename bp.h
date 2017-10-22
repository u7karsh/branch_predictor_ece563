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
typedef  struct  _bpControllerT       *bpControllerPT;
typedef  struct  _tagT                *tagPT;
typedef  struct  _tagStoreT           *tagStorePT;

// Enum to hold direction like read/write
typedef enum {
   BP_PATH_NOT_TAKEN                         = 0,
   BP_PATH_TAKEN                             = 1,
}bpPathT;

typedef enum{
   BP_TYPE_BIMODAL                           = 0,
   BP_TYPE_GSHARE                            = 1,
   BP_TYPE_HYBRID                            = 2,
   BP_TYPE_BTB                               = 3,
}bpTypeT;

typedef struct _bpControllerT{
   bpTypeT               type;
   bpPT                  bpBimodalP;
   bpPT                  bpGshareP;
   int                   k;
   int                   kMask;
   // Chooser table for hybrid predictor
   int                   sizeChooserTable;
   int                   *chooserTable;

   // Metrics
   int                   predictions;
   int                   misPredictions;
}bpControllerT;

// Branch predictor structure.
typedef struct _bpT{
   /*
    * Configutration params
    */
   // Placeholder for name
   char                  name[128];

   boolean               btbPresent;
   int                   btbSize;
   int                   btbIndexSize;
   int                   btbIndexMask;
   int                   btbAssoc;
   int                   btbSets;
   tagStorePT            *tagStoreP;
   //-------------------- BIMODAL/GSHARE BEGIN -------------------------
   // Number of bits used to represent prediction table
   int                   m;

   int                   n;
   bpTypeT               type;
   int                   gShareShift;
   int                   nMsbSetMask;
   // Mask to calculate index from address
   int                   mMask;

   int                   nMask;

   int                   sizePredictionTable;

   int                   *predictionTable;

   // Register for GShare predictor
   int                   globalBrHistoryReg;
   //-------------------- BIMODAL/GSHARE END ---------------------------
}bpT;

typedef struct _tagT{
   int               tag;
   boolean           valid;
   
   int               counter;
}tagT;

// Tag store unit cell
typedef struct _tagStoreT{
   tagPT             *rowP;
}tagStoreT;

// Function prototypes. This part can be automated
// lets keep hardcoded as of now
bpPT  branchPredictorInit(   
      char*              name, 
      int                m,
      int                n,
      bpTypeT            type,
      int                btbSize,
      int                btbAssoc
      );
// bp funcs
void bpGetIndexTag( bpPT bpP, int address, int* indexBpP, int* indexBtbP, int* tagP );
void bpBtbHitUpdateLRU( bpPT bpP, int index, int setIndex );
int bpBtbFindReplacementUpdateCounterLRU( bpPT bpP, int index, int tag, int overrideSetIndex, int doOverride );
bpPathT bpPredict( bpPT bpP, int indexBp, int indexBtb, int tag, boolean *update );
void bpUpdatePredictionTable( bpPT bpP, int index, bpPathT actual );
void bpUpdateGlobalBrHistoryTable( bpPT bpP, bpPathT actual );
void bpPrintPredictionTable( bpPT bpP );
void bpBtbPrintContents( bpPT bpP );

// Controller funcs
bpControllerPT bpCreateController( bpPT bpBimodalP, bpPT bpGshareP, int k, bpTypeT type );
void bpControllerProcess( bpControllerPT contP, int address, bpPathT actual );
void bpControllerUpdateChooserTable( bpControllerPT contP, 
                                     int chooserIndex, 
                                     bpPathT bimodalPred, 
                                     bpPathT gsharePred, 
                                     bpPathT actual 
                                   );
int bpControllerGetChooserIndex( bpControllerPT contP, int address );
bpTypeT bpControllerHybridGetType( bpControllerPT contP, int index );
void bpControllerPrintChooserTable( bpControllerPT contP );
void bpControllerGetMetrics( bpControllerPT contP, int* predictions, int* misPredictions, double* misPredictionRate );

#endif
