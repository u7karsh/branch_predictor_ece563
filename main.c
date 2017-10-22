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
      if( taken == 't' || taken == 'T' ){
         // Taken
         bpControllerProcess( contP, address, BP_PATH_TAKEN );
      } else{
         // Not taken
         bpControllerProcess( contP, address, BP_PATH_NOT_TAKEN );
      }
   } while( !feof(fp) );
}

void doBtbPrint( bpPT bpP, int pred, int misPred, double misPredRate )
{
   if( bpP == NULL )      return;
   if( !bpP->btbPresent ) return;

   int btbPred, btbMisPred;
   bpBtbGetMetrics( bpP, &btbPred, &btbMisPred );
   printf("size of BTB: %d\n", bpP->btbSize);
   printf("number of branches: %d\n", pred);
   printf("number of predictions from branch predictor: %d\n", pred - btbPred);
   printf("number of mispredictions from branch predictor: %d\n", misPred - btbMisPred);
   printf("number of branches miss in BTB and taken: %d\n", btbMisPred);
   printf("total mispredictions: %d\n", misPred);
   printf("misprediction rate: %0.2f%%\n", misPredRate * 100.0);
   printf("\nFINAL BTB CONTENTS\n");
   bpBtbPrintContents( bpP );
}

void printStats( bpControllerPT contP, boolean printBtb )
{
   int pred, misPred;
   double misPredRate;

   printf("OUTPUT\n");

   bpControllerGetMetrics( contP, &pred, &misPred, &misPredRate );
   if( printBtb ){
      doBtbPrint( contP->bpBimodalP, pred, misPred, misPredRate );
      doBtbPrint( contP->bpGshareP , pred, misPred, misPredRate );
   } else{
      printf("number of predictions: %d\n", pred);
      printf("number of mispredictions: %d\n", misPred);
      printf("misprediction rate: %0.2f%%\n", misPredRate * 100.0);
   }

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
   int k                   = 0;
   int mGshare             = 0;
   int mBimodal            = 0;
   int n                   = 0;
   int btbAssoc            = 0;
   int btbSize             = 0;
   bpTypeT controllerType  = BP_TYPE_BIMODAL;
   printf("COMMAND\n");
   for( int i = 0; i < argc; i++ )
      printf("%s ", argv[i]);
   printf("\n");

   if ( strcmp( argv[1], "bimodal" ) == 0 ){
      ASSERT( argc < 6, "Number of arguments(=%d) less than desired(=6)", argc );
      mBimodal             = atoi( argv[2] );
      btbSize              = atoi( argv[3] );
      btbAssoc             = atoi( argv[4] );
      sprintf( traceFile, "%s", argv[5] );
      controllerType       = BP_TYPE_BIMODAL;
   } else if( strcmp( argv[1], "gshare" ) == 0 ){
      ASSERT( argc < 7, "Number of arguments(=%d) less than desired(=7)", argc );
      mGshare              = atoi( argv[2] );
      n                    = atoi( argv[3] );
      btbSize              = atoi( argv[4] );
      btbAssoc             = atoi( argv[5] );
      sprintf( traceFile, "%s", argv[6] );
      controllerType       = BP_TYPE_GSHARE;
   } else if( strcmp( argv[1], "hybrid" ) == 0 ){
      ASSERT( argc < 9, "Number of arguments(=%d) less than desired(=9)", argc );
      k                    = atoi( argv[2] );
      mGshare              = atoi( argv[3] );
      n                    = atoi( argv[4] );
      mBimodal             = atoi( argv[5] );
      btbSize              = atoi( argv[6] );
      btbAssoc             = atoi( argv[7] );
      sprintf( traceFile, "%s", argv[8] );
      controllerType       = BP_TYPE_HYBRID;
   } else{
      ASSERT( TRUE, "Illegal predictor requested: %s", argv[1] );
   }

   FILE* fp                = fopen( traceFile, "r" ); 
   ASSERT(!fp, "Unable to read file: %s\n", traceFile);
   bpPT bpBimodalP         = branchPredictorInit( "bimodal", mBimodal, n, BP_TYPE_BIMODAL, btbSize, btbAssoc );
   bpPT bpGshareP          = branchPredictorInit( "gshare" , mGshare , n, BP_TYPE_GSHARE , btbSize, btbAssoc );
   bpControllerPT contP    = bpCreateController( bpBimodalP, bpGshareP, k, controllerType );

   doTrace( contP, fp );
   printStats( contP, btbSize > 0 );
}
