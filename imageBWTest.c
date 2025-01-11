// imageBWTest - A program that performs some image processing.
//
// This program is an example use of the imageBW module,
// a programming project for the course AED, DETI / UA.PT
//
// You may freely use and modify this code, NO WARRANTY, blah blah,
// as long as you give proper credit to the original and subsequent authors.
//
// The AED Team <jmadeira@ua.pt, jmr@ua.pt, ...>
// 2024

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imageBW.h"
#include "instrumentation.h"

int main(int argc, char* argv[]) {
  // To initalize operation counters
  ImageInit();

  // Creating and displaying some images
  Image white_image = ImageCreate(8, 8, WHITE);
  // ImageRAWPrint(white_image);

  Image black_image = ImageCreate(8, 8, BLACK);
  // ImageRAWPrint(black_image);

  Image image_1 = ImageNEG(white_image);
  Image image_2 = ImageNEG(black_image);
  // ImageRAWPrint(image_1);

  // for (uint32 i = 2; i <= 100; i++) {
  //   for (uint32 j = i-1; j <= i; j++) {
  //     if (i%j != 0)
  //       continue;

  //     InstrReset();
  //     // InstrCalibrate();
  //     Image image_cb = ImageCreateChessboard(i, i, j, WHITE);
  //     printf("Chessboard Size: %dx%d; Square Length: %d; Number of Runs: %d; Memory Occupied: %ld\n", i, i, j, (i/j)*i, (sizeof(int) * (2 + i/j) * i) + (sizeof(uint32) * 2));
  //     InstrPrint();
  //   }
  // }


  Image image_cb = ImageCreateChessboard(8, 8, 1, WHITE);
  // ImageRLEPrint(image_cb);
  ImageRAWPrint(image_cb);

  // Image image_2 = ImageReplicateAtBottom(white_image, black_image);
  // ImageRAWPrint(image_2);

  for (uint32 i = 3; i <= 100; i++) {
    Image image_and_1 = ImageCreate(i, i, BLACK);
    Image image_and_2 = ImageCreate(i, i, BLACK);
    printf("Size: %dx%d\n", i,i);
    Image AND = ImageAND(image_and_1,image_and_2);
  }

  printf("image_1 AND image_1\n");
  Image image_3 = ImageAND(image_1, image_1);
  ImageRAWPrint(image_3);

  printf("image_1 AND image_2\n");
  Image image_4 = ImageAND(image_1, image_2);
  ImageRAWPrint(image_4);

  printf("image_1 AND image_cb\n");
  Image image_cb_1 = ImageAND(image_1, image_cb);
  ImageRAWPrint(image_cb_1);

  printf("image_2 AND image_cb\n");
  Image image_cb_2 = ImageAND(image_2, image_cb);
  ImageRAWPrint(image_cb_2);



  printf("image_1 OR image_2\n");
  Image image_5 = ImageOR(image_1, image_2);
  ImageRAWPrint(image_5);

  printf("image_1 XOR image_1\n");
  Image image_6 = ImageXOR(image_1, image_1);
  ImageRAWPrint(image_6);

  printf("image_1 XOR image_2\n");
  Image image_7 = ImageXOR(image_1, image_2);
  ImageRAWPrint(image_7);

  /*** UNCOMMENT TO TEST THE NEXT FUNCTIONS  

  Image image_8 = ImageReplicateAtRight(image_6, image_7);
  ImageRAWPrint(image_8);

  Image image_9 = ImageReplicateAtRight(image_6, image_6);
  ImageRAWPrint(image_9);
    ***/

  Image image_10 = ImageHorizontalMirror(image_1);
  ImageRAWPrint(image_10);
  Image image_11 = ImageHorizontalMirror(image_cb);
  ImageRAWPrint(image_11);
  


  Image image_12 = ImageVerticalMirror(image_cb);
  ImageRAWPrint(image_12);

  // Saving in PBM format
 // ImageSave(image_7, "image7.pbm");
  //ImageSave(image_8, "image8.pbm");


  // Housekeeping
  ImageDestroy(&white_image);
  ImageDestroy(&black_image);
  ImageDestroy(&image_1);



  ImageDestroy(&image_2);
  ImageDestroy(&image_3);
  ImageDestroy(&image_4);
  ImageDestroy(&image_5);
  ImageDestroy(&image_6);
  ImageDestroy(&image_7);
  // ImageDestroy(&image_8);
  // ImageDestroy(&image_9);
  ImageDestroy(&image_10);
  ImageDestroy(&image_11);



  return 0;
}
