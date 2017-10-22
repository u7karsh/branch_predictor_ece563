/*H**********************************************************************
* FILENAME    :       main.c 
* DESCRIPTION :       Program2 for ECE 563. Generic branch predictor
* NOTES       :       Entry to sim program
*
* AUTHOR      :       Utkarsh Mathur           START DATE :    20 Oct 17
*
* CHANGES :
*
*H***********************************************************************/


#include "all.h"
#include "bp.h"

void doTrace( bpControllerPT contP, FILE* fp )
{
   char taken;
   int address;
   int count = 0;
   do{
      int bytesRead = fscanf( fp, "%x %c\n", &address, &taken );
      // Just to safeguard on byte reading
      ASSERT(bytesRead <= 0, "fscanf read nothing!");
      count++;
      //printf("%d. PC: %x %c\n", count, address, taken);
      if( taken == 't' || taken == 'T' ){
         // Taken
         bpControllerProcess( contP, address, BP_PATH_TAKEN );
      } else{
         // Not taken
         bpControllerProcess( contP, address, BP_PATH_NOT_TAKEN );
      }
   } while( !feof(fp) );
}

void printStats( bpControllerPT contP )
{
   int pred, misPred;
   double misPredRate;
   bpControllerGetMetrics( contP, &pred, &misPred, &misPredRate );
   printf("number of predictions: %d\n", pred);
   printf("number of mispredictions: %d\n", misPred);
   printf("misprediction rate: %0.2f%%\n", misPredRate * 100.0);
   printf("FINAL BTB CONTENTS\n");
   bpBtbPrintContents( contP->bpGshareP );
   if( contP->type == BP_TYPE_HYBRID ){
      printf("FINAL CHOOSER CONTENTS\n");
      bpControllerPrintChooserTable( contP );
   }
   if( contP->type == BP_TYPE_GSHARE || contP->type == BP_TYPE_HYBRID ){
      printf("FINAL GSHARE CONTENTS\n");
      bpPrintPredictionTable( contP->bpGshareP );
   }
   if( contP->type == BP_TYPE_BIMODAL || contP->type == BP_TYPE_HYBRID ){
      printf("FINAL BIMODAL CONTENTS\n");
      bpPrintPredictionTable( contP->bpBimodalP );
   }
}

int main( int argc, char** argv )
{
   char traceFile[128];
   int k                   = atoi(argv[1]);
   int mGshare             = atoi(argv[2]);
   int mBimodal            = atoi(argv[3]);
   int n                   = atoi(argv[4]);
   int assocGshare         = atoi(argv[5]);
   int assocBimodal        = atoi(argv[6]);
   sprintf( traceFile, "%s", argv[7] );

   FILE* fp                = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);
   bpPT bpBimodalP         = branchPredictorInit( "bimodal", mBimodal, n, BP_TYPE_BIMODAL, 2048, assocBimodal);
   bpPT bpGshareP          = branchPredictorInit( "gshare" , mGshare , n, BP_TYPE_GSHARE, 2048, assocGshare);
   bpControllerPT contP    = bpCreateController( bpBimodalP, bpGshareP, k, BP_TYPE_BIMODAL );

   doTrace( contP, fp );
   printStats( contP );
}
