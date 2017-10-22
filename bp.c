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
      bpTypeT            type,
      int                btbSize,
      int                btbAssoc
      )
{
   // Calloc the mem to reset all vars to 0
   bpPT bpP                          = (bpPT) calloc( 1, sizeof(bpT) );
   if( type == BP_TYPE_GSHARE || type == BP_TYPE_HYBRID )
      ASSERT( n > m, "Illegal value of mGshare, n pair ( n !< mGshare )" );

   sprintf( bpP->name, "%s", name );
   bpP->m                            = m;
   bpP->n                            = n;
   bpP->type                         = type;
   bpP->gShareShift                  = m - n;
   // Lower 2 bits are reserved and not used in index
   bpP->mMask                        = utilCreateMask( m, 0 );


   bpP->sizePredictionTable          = (m > 0) ? pow(2, m) : 0;

   bpP->nMask                        = utilCreateMask( n, 0 );
   bpP->nMsbSetMask                  = 1 << (n - 1);

   bpP->predictionTable              = (int*) calloc( bpP->sizePredictionTable, sizeof(int) );

   // Initialize all to 2 (weakly taken)
   for( int i = 0; i < bpP->sizePredictionTable; i++ ){
      bpP->predictionTable[i]  = 2;
   }

   // BTB
   bpP->btbPresent                   = TRUE;
   bpP->btbSize                      = btbSize;
   bpP->btbAssoc                     = btbAssoc;
   bpP->btbSets                      = ceil( (double) btbSize / (double) (btbAssoc * 4) );
   bpP->btbIndexSize                 = CLOG2( bpP->btbSets );
   bpP->btbIndexMask                 = utilCreateMask( bpP->btbIndexSize, 0 );
   bpP->tagStoreP                    = (tagStorePT*) calloc(bpP->btbSets, sizeof(tagStorePT));
   for( int index = 0; index < bpP->btbSets; index++ ){
       bpP->tagStoreP[index]         = (tagStorePT) calloc(1, sizeof(tagStoreT));
       bpP->tagStoreP[index]->rowP   = (tagPT*) calloc(bpP->btbAssoc, sizeof(tagPT));
       for( int setIndex = 0; setIndex < bpP->btbAssoc; setIndex++ ){
          bpP->tagStoreP[index]->rowP[setIndex] = (tagPT)  calloc(1, sizeof(tagT));

          // Update the counter values to comply with LRU defaults
          bpP->tagStoreP[index]->rowP[setIndex]->counter = setIndex;
       }
   }

   return bpP;
}

void bpGetIndexTag( bpPT bpP, int address, int* indexBpP, int* indexBtbP, int* tagP )
{
   int mask    = bpP->mMask;
   int indexBp = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, mask );
   if( bpP->type == BP_TYPE_GSHARE ){
      ASSERT( bpP->n <= 0, "Illegal value of n(=%d) for type GSHARE", bpP->n );
      // GShare
      // the current n-bit global branch history reg is Xored with uppermost n bits of PC
      indexBp  = ( bpP->globalBrHistoryReg << bpP->gShareShift ) ^ indexBp;
   }
   *indexBpP   = indexBp & mask;
   *indexBtbP  = utilShiftAndMask( address, BP_LOWER_RSVD_CNT, bpP->btbIndexMask );
   *tagP       = address >> (bpP->btbIndexSize + 2);
}

void bpBtbHitUpdateLRU( bpPT bpP, int index, int setIndex )
{
   tagPT     *rowP = bpP->tagStoreP[index]->rowP;

   // Increment the counter of other blocks whose counters are less
   // than the referenced block's old counter value
   for( int assocIndex = 0; assocIndex < bpP->btbAssoc; assocIndex++ ){
      if( rowP[assocIndex]->counter < rowP[setIndex]->counter )
         rowP[assocIndex]->counter++;
   }

   // Set the referenced block's counter to 0 (Most recently used)
   rowP[setIndex]->counter = 0;
}

int bpBtbFindReplacementUpdateCounterLRU( bpPT bpP, int index, int tag, int overrideSetIndex, int doOverride )
{
   tagPT   *rowP = bpP->tagStoreP[index]->rowP;
   int replIndex = 0;

   // Place data at max counter value and reset counter to 0
   // Increment all counters
   if( doOverride ){
      // Update the counter values just as if it was a hit for this index
      bpBtbHitUpdateLRU( bpP, index, overrideSetIndex );
      replIndex  = overrideSetIndex;

   } else{
      // Speedup: do a increment % assoc to all elements and update tag
      // for value 0
      for( int assocIndex = 0; assocIndex < bpP->btbAssoc; assocIndex ++ ){
         rowP[assocIndex]->counter   = (rowP[assocIndex]->counter + 1) % bpP->btbAssoc;
         if( rowP[assocIndex]->counter == 0 )
            replIndex              = assocIndex;
      }
   }

   return replIndex;
}


bpPathT bpPredict( bpPT bpP, int indexBp, int indexBtb, int tag, boolean *update )
{
   *update            = TRUE;
   if( bpP->btbPresent ){
      // Btb is present
      boolean hit     = FALSE;
      tagPT *rowP     = bpP->tagStoreP[indexBtb]->rowP;
      // Find the data
      for( int assocIndex = 0; assocIndex < bpP->btbAssoc; assocIndex++ ){
         if( rowP[assocIndex]->valid == 1 && rowP[assocIndex]->tag == tag ){
            hit       = TRUE;
            //printf("BTB HIT\n");
            bpBtbHitUpdateLRU( bpP, indexBtb, assocIndex );
            break;
         }
      }

      // In case of hit, do as intended
      // In case of miss, predict not taken and do a replacement
      if( !hit ){
         // Irrespective of replacement policy, check if there exists
         // a block with valid bit unset
         int success   = 0;
         int assocIndex;
         for( assocIndex = 0; assocIndex < bpP->btbAssoc; assocIndex++ ){
            if( rowP[assocIndex]->valid == 0 ){
               success = 1;
               break;
            }
         }
         assocIndex              = bpBtbFindReplacementUpdateCounterLRU( bpP, indexBtb, tag, assocIndex, success );
         rowP[assocIndex]->tag   = tag;
         rowP[assocIndex]->valid = 1;
         *update                 = FALSE;
         return BP_PATH_NOT_TAKEN;
      }
   } 

   int predictionCounter    = bpP->predictionTable[indexBp];

   // If the counter value is greater than or equal to 2, predict taken, else not taken
   return ( predictionCounter >= 2 ) ? BP_PATH_TAKEN : BP_PATH_NOT_TAKEN;
}

// Counter is incremented if branch was taken, decremented if not taken
// Counter saturates at [0, 3]
void bpUpdatePredictionTable( bpPT bpP, int index, bpPathT actual )
{
   int counter                 = bpP->predictionTable[index];
   //printf("GSHARE index: %d old value: %d", index, counter);
   counter                    += ( actual == BP_PATH_TAKEN ) ? 1 : -1;

   // Saturate                
   counter                     = ( counter > 3 ) ? 3 : counter;
   counter                     = ( counter < 0 ) ? 0 : counter;
   
   //printf(" new value %d\n", counter);
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

void bpBtbPrintContents( bpPT bpP )
{
   if( !bpP ) return;
   for( int setIndex = 0; setIndex < bpP->btbSets; setIndex++ ){
      printf("set\t\t%d:\t\t", setIndex);
      tagPT *rowP = bpP->tagStoreP[setIndex]->rowP;
      for( int assocIndex = 0; assocIndex < bpP->btbAssoc; assocIndex++ ){
         printf("%x\t", rowP[assocIndex]->tag);
      }
      printf("\n");
   }
}


//------------------- CONTROLLER FUNCS ---------------------
bpControllerPT bpCreateController( bpPT bpBimodalP, bpPT bpGshareP, int k, bpTypeT type )
{
   bpControllerPT contP       = (bpControllerPT) calloc( 1, sizeof(bpControllerT) );
   contP->type                = type;
   contP->bpBimodalP          = bpBimodalP;
   contP->bpGshareP           = bpGshareP;

   contP->kMask               = utilCreateMask( k, 0 );
   contP->sizeChooserTable    = ( k > 0 ) ? pow(2, k) : 0;
   contP->chooserTable        = (int*) calloc( contP->sizeChooserTable, sizeof(int) );

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
      int indexBp, indexBtb, tag;
      boolean update;
      bpGetIndexTag( contP->bpBimodalP, address, &indexBp, &indexBtb, &tag );
      predicted                = bpPredict( contP->bpBimodalP, indexBp, indexBtb, tag, &update );
      if( update )
         bpUpdatePredictionTable( contP->bpBimodalP, indexBp, actual );
   } else if( type == BP_TYPE_GSHARE ){
      int indexBp, indexBtb, tag; 
      boolean update;
      bpGetIndexTag( contP->bpGshareP, address, &indexBp, &indexBtb, &tag );
      predicted                = bpPredict( contP->bpGshareP, indexBp, indexBtb, tag, &update );
      if( update ){
         bpUpdatePredictionTable( contP->bpGshareP, indexBp, actual );
         bpUpdateGlobalBrHistoryTable( contP->bpGshareP, actual );
      }
   } else{
      // Hybrid
      // Get predictions from both models
      int bimodalIndexBp, bimodalIndexBtb, bimodalTag;
      boolean updateBimodal;
      bpGetIndexTag( contP->bpBimodalP, address, &bimodalIndexBp, &bimodalIndexBtb, &bimodalTag );
      bpPathT bimodalPred      = bpPredict( contP->bpBimodalP, bimodalIndexBp, bimodalIndexBtb, bimodalTag, &updateBimodal );

      int gshareIndexBp, gshareIndexBtb, gshareTag;
      boolean updateGshare;
      bpGetIndexTag( contP->bpGshareP, address, &gshareIndexBp, &gshareIndexBtb, &gshareTag );
      bpPathT gsharePred       = bpPredict( contP->bpGshareP, gshareIndexBp, gshareIndexBtb, gshareTag, &updateGshare );

      // Do a lookup from chooser table
      int chooserIndex         = bpControllerGetChooserIndex( contP, address );
      bpTypeT selectedType     = bpControllerHybridGetType( contP, chooserIndex );

      predicted                = ( selectedType == BP_TYPE_BIMODAL ) ? bimodalPred : gsharePred;

      if( selectedType == BP_TYPE_BIMODAL ){
         // Update bimodal
         if( updateBimodal )
            bpUpdatePredictionTable( contP->bpBimodalP, bimodalIndexBp, actual );
      } else{
         // Update Gshare
         if( updateGshare )
            bpUpdatePredictionTable( contP->bpGshareP, gshareIndexBp, actual );
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
