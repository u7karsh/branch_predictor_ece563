/*H**********************************************************************
* FILENAME    :       bp.c 
* DESCRIPTION :       Consists all branch predictor related operations
* NOTES       :       *Caution* Data inside the original bpP
*                     might change based on functions.
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    20 Oct 17
*
* CHANGES :
*
*H***********************************************************************/

#include "bp.h"

// Since this is a small proj, add all utils in this
// file instead of a separate file
//-------------- UTILITY BEGIN -----------------

#define LOG2(A)      log(A) / log(2)
// This is a ceil LOG2
#define CLOG2(A)     ceil( LOG2(A) )

// Creates a mask with lower nBits set to 1 and left shifts it by offset
int  utilCreateMask( int nBits, int offset )
{
   int mask = 0, index;
   for( index=0; index<nBits; index++ )
      mask |= 1 << index;

   return mask << offset;
}

int utilShiftAndMask( int input, int shiftValue, int mask )
{
   return (input >> shiftValue) & mask;
}

//-------------- UTILITY END   -----------------

// Allocates and inits all internal variables
bpPT  branchPredictorInit(   
      char*              name, 
      int                m,
      int                n,
      bpTypeT            type
      )
{
   // Calloc the mem to reset all vars to 0
   bpPT bpP                          = (bpPT) calloc( sizeof(bpT), 1 );
   if( type == BP_TYPE_GSHARE || type == BP_TYPE_HYBRID )
      ASSERT( n > m, "Illegal value of mGshare, n pair ( n !< mGshare )" );

   sprintf( bpP->name, "%s", name );
   bpP->m                            = m;
   bpP->n                            = n;
   bpP->type                         = type;
   bpP->gShareShift                  = m - n;
   // Lower 2 bits are reserved and not used in index
   bpP->m                            = utilCreateMask( m, 0 );


   bpP->sizePredictionTable          = (m > 0) ? pow(2, m) : 0;

   bpP->nMask                        = utilCreateMask( n, 0 );
   bpP->nMsbSetMask                  = 1 << (n - 1);

   bpP->predictionTable              = (int*) calloc( sizeof(int), bpP->sizePredictionTable );


   // Initialize all to 2 (weakly taken)
   for( int i = 0; i < bpP->sizePredictionTable; i++ ){
      bpP->predictionTable[i]  = 2;
   }
   return bpP;
}

int bpGetIndex( bpPT bpP, int address )
{
   int mask    = bpP->m;
   int index   = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, mask );
   if( bpP->type == BP_TYPE_GSHARE ){
      ASSERT( bpP->n <= 0, "Illegal value of n(=%d) for type GSHARE", bpP->n );
      // GShare
      // the current n-bit global branch history reg is Xored with uppermost n bits of PC
      index    = ( bpP->globalBrHistoryReg << bpP->gShareShift ) ^ index;
   }
   return index & mask;
}

inline bpPathT bpPredict( bpPT bpP, int index )
{
   int predictionCounter       = bpP->predictionTable[index];

   // If the counter value is greater than or equal to 2, predict taken, else not taken
   return ( predictionCounter >= 2 ) ? BP_PATH_TAKEN : BP_PATH_NOT_TAKEN;
}

// Counter is incremented if branch was taken, decremented if not taken
// Counter saturates at [0, 3]
void bpUpdatePredictionTable( bpPT bpP, int index, bpPathT actual )
{
   int counter                 = bpP->predictionTable[index];
   counter                    += ( actual == BP_PATH_TAKEN ) ? 1 : -1;

   // Saturate                
   counter                     = ( counter > 3 ) ? 3 : counter;
   counter                     = ( counter < 0 ) ? 0 : counter;
   
   bpP->predictionTable[index] = counter;
}

void bpUpdateGlobalBrHistoryTable( bpPT bpP, bpPathT actual )
{
   ASSERT( bpP->n <= 0, "Illegal value of n(=%d) for type GSHARE/HYBRID", bpP->n );
   // Right shift by 1 and place the branch outcome to MSB of reg
   bpP->globalBrHistoryReg   >>= 1;
   if( actual == BP_PATH_TAKEN ){
      bpP->globalBrHistoryReg |= bpP->nMsbSetMask;
      bpP->globalBrHistoryReg &= bpP->nMask;
   }
}

void bpPrintPredictionTable( bpPT bpP )
{
   for( int i = 0; i < bpP->sizePredictionTable; i++ ){
      printf("%d %d\n", i, bpP->predictionTable[i]);
   }
}

//------------------- CONTROLLER FUNCS ---------------------
bpControllerPT bpCreateController( bpPT bpBimodalP, bpPT bpGshareP, int k, bpTypeT type )
{
   bpControllerPT contP       = (bpControllerPT) calloc( sizeof(bpControllerT), 1 );
   contP->type                = type;
   contP->bpBimodalP          = bpBimodalP;
   contP->bpGshareP           = bpGshareP;

   contP->kMask               = utilCreateMask( k, 0 );
   contP->sizeChooserTable    = ( k > 0 ) ? pow(2, k) : 0;
   contP->chooserTable        = (int*) calloc( sizeof(int), contP->sizeChooserTable );

   // Initialize all with 1
   for( int i = 0; i < contP->sizeChooserTable; i++ ){
      contP->chooserTable[i]  = 1;
   }
   return contP;
}

void bpControllerProcess( bpControllerPT contP, int address, bpPathT actual )
{
   bpTypeT type                = contP->type;
   bpPathT predicted;

   // Predict based on predictor
   if( type == BP_TYPE_BIMODAL ){
      int index                = bpGetIndex( contP->bpBimodalP, address );
      predicted                = bpPredict( contP->bpBimodalP, index );
      bpUpdatePredictionTable( contP->bpBimodalP, index, actual );
   } else if( type == BP_TYPE_GSHARE ){
      int index                = bpGetIndex( contP->bpGshareP, address );
      predicted                = bpPredict( contP->bpGshareP, index );
      bpUpdatePredictionTable( contP->bpGshareP, index, actual );
      bpUpdateGlobalBrHistoryTable( contP->bpGshareP, actual );
   } else{
      // Hybrid
      // Get predictions from both models
      int bimodalIndex         = bpGetIndex( contP->bpBimodalP, address );
      bpPathT bimodalPred      = bpPredict( contP->bpBimodalP, bimodalIndex );

      int gshareIndex          = bpGetIndex( contP->bpGshareP, address );
      bpPathT gsharePred       = bpPredict( contP->bpGshareP, gshareIndex );

      // Do a lookup from chooser table
      int chooserIndex         = bpControllerGetChooserIndex( contP, address );
      bpTypeT selectedType     = bpControllerHybridGetType( contP, chooserIndex );

      predicted                = ( selectedType == BP_TYPE_BIMODAL ) ? bimodalPred : gsharePred;

      if( selectedType == BP_TYPE_BIMODAL ){
         // Update bimodal
         bpUpdatePredictionTable( contP->bpBimodalP, bimodalIndex, actual );
      } else{
         // Update Gshare
         bpUpdatePredictionTable( contP->bpGshareP, gshareIndex, actual );
      }

      // Global Br updates irrespective of decision
      bpUpdateGlobalBrHistoryTable( contP->bpGshareP, actual );
   
      bpControllerUpdateChooserTable( contP, chooserIndex, bimodalPred, gsharePred, actual );
   }

   contP->predictions++;
   // Update the miss predictions
   contP->misPredictions        += (actual != predicted);
}

void bpControllerUpdateChooserTable( bpControllerPT contP, 
                                     int chooserIndex, 
                                     bpPathT bimodalPred, 
                                     bpPathT gsharePred, 
                                     bpPathT actual 
                                   )
{
   int increment = 0;
   int counter   = contP->chooserTable[chooserIndex];
   // Both Correct or both incorrect
   if( ( bimodalPred == actual && gsharePred == actual ) ||
       ( bimodalPred != actual && gsharePred != actual ) ){
      increment  = 0;
   } else if( gsharePred == actual ){
      increment  = 1;
   }
   else{
      increment  = -1;
   }

   counter      += increment;

   // Saturate
   counter       = ( counter > 3 ) ? 3 : counter;
   counter       = ( counter < 0 ) ? 0 : counter;

   contP->chooserTable[chooserIndex]  = counter;
}

inline int bpControllerGetChooserIndex( bpControllerPT contP, int address )
{
   // Determine the branch's index k+1 to 2
   int index = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, contP->kMask );
   return index;
}

inline bpTypeT bpControllerHybridGetType( bpControllerPT contP, int index )
{
   int counter = contP->chooserTable[index];
   return ( counter >= 2 ) ? BP_TYPE_GSHARE : BP_TYPE_BIMODAL;
}

//----------------------------------------------------------------------
void bpControllerPrintChooserTable( bpControllerPT contP )
{
   for( int i = 0; i < contP->sizeChooserTable; i++ ){
      printf("%d %d\n", i, contP->chooserTable[i]);
   }
}

void bpControllerGetMetrics( bpControllerPT contP, int* predictions, int* misPredictions, double* misPredictionRate )
{
   *predictions       = contP->predictions;
   *misPredictions    = contP->misPredictions;
   *misPredictionRate = (double) ( contP->misPredictions ) / (double) contP->predictions;
}
