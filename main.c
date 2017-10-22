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

void doTrace( bpPT bpP, FILE* fp )
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
         bpProcess( bpP, address, BP_PATH_TAKEN );
      } else{
         // Not taken
         bpProcess( bpP, address, BP_PATH_NOT_TAKEN );
      }
   } while( !feof(fp) );
}

void printStats( bpPT bpP )
{
   int pred, misPred;
   double misPredRate;
   bpGetMetrics( bpP, &pred, &misPred, &misPredRate );
   printf("number of predictions: %d\n", pred);
   printf("number of mispredictions: %d\n", misPred);
   printf("misprediction rate: %0.2f%%\n", misPredRate * 100.0);
   printf("FINAL CHOOSER CONTENTS\n");
   bpPrintChooserTable( bpP );
   if( bpP->type == BP_TYPE_GSHARE || bpP->type == BP_TYPE_HYBRID ){
      printf("FINAL GSHARE CONTENTS\n");
      bpPrintPredictionTableGshare( bpP );
   }
   if( bpP->type == BP_TYPE_BIMODAL || bpP->type == BP_TYPE_HYBRID ){
      printf("FINAL BIMODAL CONTENTS\n");
      bpPrintPredictionTableBimodal( bpP );
   }
}

int main( int argc, char** argv )
{
   char traceFile[128];
   int k                   = atoi(argv[1]);
   int mGshare             = atoi(argv[2]);
   int mBimodal            = atoi(argv[3]);
   int n                   = atoi(argv[4]);
   sprintf( traceFile, "%s", argv[5] );

   FILE* fp                = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);
   bpPT bpP                = branchPredictorInit( "bimodal", mBimodal, mGshare, n, k, BP_TYPE_HYBRID );

   doTrace( bpP, fp );
   printStats( bpP );
}
