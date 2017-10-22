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
      int                mBimodal,
      int                mGshare,
      int                n,
      int                k,
      bpTypeT            type
      )
{
   // Calloc the mem to reset all vars to 0
   bpPT bpP                          = (bpPT) calloc( sizeof(bpT), 1 );
   if( type == BP_TYPE_GSHARE || type == BP_TYPE_HYBRID )
      ASSERT( n > mGshare, "Illegal value of mGshare, n pair ( n !< mGshare )" );

   sprintf( bpP->name, "%s", name );
   bpP->mBimodal                     = mBimodal;
   bpP->mGshare                      = mGshare;
   bpP->n                            = n;
   bpP->type                         = type;
   bpP->gShareShift                  = mGshare - n;
   // Lower 2 bits are reserved and not used in index
   bpP->mGshareMask                  = utilCreateMask( mGshare, 0 );
   bpP->mBimodalMask                 = utilCreateMask( mBimodal, 0 );

   bpP->kMask                        = utilCreateMask( k, 0 );
   bpP->sizeChooserTable             = ( k > 0 ) ? pow(2, k) : 0;
   bpP->chooserTable                 = (int*) calloc( sizeof(int), bpP->sizeChooserTable );

   bpP->sizePredictionTableBimodal   = (mBimodal > 0) ? pow(2, mBimodal) : 0;
   bpP->sizePredictionTableGshare    = (mGshare > 0)  ? pow(2, mGshare)  : 0;

   bpP->nMask                        = utilCreateMask( n, 0 );
   bpP->nMsbSetMask                  = 1 << (n - 1);

   bpP->predictionTableBimodal       = (int*) calloc( sizeof(int), bpP->sizePredictionTableBimodal );
   bpP->predictionTableGshare        = (int*) calloc( sizeof(int), bpP->sizePredictionTableGshare );


   // Initialize all to 2 (weakly taken)
   for( int i = 0; i < bpP->sizePredictionTableBimodal; i++ ){
      bpP->predictionTableBimodal[i] = 2;
   }
   for( int i = 0; i < bpP->sizePredictionTableGshare; i++ ){
      bpP->predictionTableGshare[i]  = 2;
   }

   // Initialize all with 1
   for( int i = 0; i < bpP->sizeChooserTable; i++ ){
      bpP->chooserTable[i]           = 1;
   }

   return bpP;
}

void bpProcess( bpPT bpP, int address, bpPathT actual )
{
   bpTypeT type                = bpP->type;
   int chooserIndex;
   // Variables to be used only for hybrid
   int indexRemaining;
   bpPathT predictedRemaining;

   if( bpP->type == BP_TYPE_HYBRID ){
      chooserIndex             = bpGetChooserIndex( bpP, address );
      type                     = bpHybridGetType( bpP, chooserIndex );
      // Predict the other part
      bpTypeT typeRemaining    = (type == BP_TYPE_BIMODAL) ? BP_TYPE_GSHARE : BP_TYPE_BIMODAL;
      indexRemaining           = bpGetIndex( bpP, address, typeRemaining );
      predictedRemaining       = bpPredict( bpP, indexRemaining, typeRemaining );
   }

   int index                   = bpGetIndex( bpP, address, type );
   bpPathT predicted           = bpPredict( bpP, index, type );

   // Update the branch predictor based on the branch's actual outcome
   bpUpdatePredictionTable( bpP, index, actual, type );

   if( bpP->type == BP_TYPE_HYBRID ){
      bpUpdateChooserTable( bpP, chooserIndex, type, predicted, predictedRemaining, actual );
   }

   bpP->predictions++;
   // Update the miss predictions
   bpP->misPredictions        += (actual != predicted);
}

void bpUpdateChooserTable( bpPT bpP, int chooserIndex, bpTypeT type, bpPathT pred1, bpPathT pred2, bpPathT actual )
{
   int increment = 0;
   int counter   = bpP->chooserTable[chooserIndex];
   // Both Correct or both incorrect
   if( ( pred1 == actual && pred2 == actual ) ||
       ( pred1 != actual && pred2 != actual ) ){
      increment  = 0;
   } else if( type == BP_TYPE_BIMODAL ){
      increment  = (pred1 == actual) ? -1 : 1;
   }
   else{
      increment  = (pred1 == actual) ? 1 : -1;
   }

   counter      += increment;

   // Saturate
   counter       = ( counter > 3 ) ? 3 : counter;
   counter       = ( counter < 0 ) ? 0 : counter;

   bpP->chooserTable[chooserIndex]  = counter;
}

inline int bpGetChooserIndex( bpPT bpP, int address )
{
   // Determine the branch's index k+1 to 2
   int index = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, bpP->kMask );
   return index;
}

inline bpTypeT bpHybridGetType( bpPT bpP, int index )
{
   int counter = bpP->chooserTable[index];
   return ( counter >= 2 ) ? BP_TYPE_GSHARE : BP_TYPE_BIMODAL;
}

int bpGetIndex( bpPT bpP, int address, bpTypeT type )
{
   int mask    = ( type == BP_TYPE_BIMODAL ) ? bpP->mBimodalMask : bpP->mGshareMask;
   int index   = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, mask );
   if( type == BP_TYPE_GSHARE ){
      ASSERT( bpP->n <= 0, "Illegal value of n(=%d) for type GSHARE", bpP->n );
      // GShare
      // the current n-bit global branch history reg is Xored with uppermost n bits of PC
      index    = ( bpP->globalBrHistoryReg << bpP->gShareShift ) ^ index;
   }
   return index & mask;
}

inline bpPathT bpPredict( bpPT bpP, int index, bpTypeT type )
{
   int predictionCounter       = (type == BP_TYPE_GSHARE) ? bpP->predictionTableGshare[index] : bpP->predictionTableBimodal[index];
   // If the counter value is greater than or equal to 2, predict taken, else not taken
   return ( predictionCounter >= 2 ) ? BP_PATH_TAKEN : BP_PATH_NOT_TAKEN;
}

// Counter is incremented if branch was taken, decremented if not taken
// Counter saturates at [0, 3]
void bpUpdatePredictionTable( bpPT bpP, int index, bpPathT actual, bpTypeT takenType )
{
   int counter                 = ( takenType == BP_TYPE_GSHARE ) ? bpP->predictionTableGshare[index] : 
                                                                   bpP->predictionTableBimodal[index];
   counter                    += ( actual == BP_PATH_TAKEN ) ? 1 : -1;

   // Saturate                
   counter                     = ( counter > 3 ) ? 3 : counter;
   counter                     = ( counter < 0 ) ? 0 : counter;
   
   if( takenType == BP_TYPE_GSHARE ){
      bpP->predictionTableGshare[index] = counter;
   } else{
      bpP->predictionTableBimodal[index]  = counter;
   }

   if( bpP->type == BP_TYPE_GSHARE || bpP->type == BP_TYPE_HYBRID ){
      ASSERT( bpP->n <= 0, "Illegal value of n(=%d) for type GSHARE/HYBRID", bpP->n );
      // Right shift by 1 and place the branch outcome to MSB of reg
      bpP->globalBrHistoryReg   >>= 1;
      if( actual == BP_PATH_TAKEN ){
         bpP->globalBrHistoryReg |= bpP->nMsbSetMask;
         bpP->globalBrHistoryReg &= bpP->nMask;
      }
   }
}

void bpPrintPredictionTableBimodal( bpPT bpP )
{
   for( int i = 0; i < bpP->sizePredictionTableBimodal; i++ ){
      printf("%d %d\n", i, bpP->predictionTableBimodal[i]);
   }
}

void bpPrintPredictionTableGshare( bpPT bpP )
{
   for( int i = 0; i < bpP->sizePredictionTableGshare; i++ ){
      printf("%d %d\n", i, bpP->predictionTableGshare[i]);
   }
}

void bpPrintChooserTable( bpPT bpP )
{
   for( int i = 0; i < bpP->sizeChooserTable; i++ ){
      printf("%d %d\n", i, bpP->chooserTable[i]);
   }
}

void bpGetMetrics( bpPT bpP, int* predictions, int* misPredictions, double* misPredictionRate )
{
   *predictions       = bpP->predictions;
   *misPredictions    = bpP->misPredictions;
   *misPredictionRate = (double) ( bpP->misPredictions ) / (double) bpP->predictions;
}
