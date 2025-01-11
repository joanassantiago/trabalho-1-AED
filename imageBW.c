/// imageBW - A simple image processing module for BW images
///           represented using run-length encoding (RLE)
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// The AED Team <jmadeira@ua.pt, jmr@ua.pt, ...>
/// 2024

// Student authors (fill in below):
// NMec: 119705
// Name: Joana Santiago
// NMec: 118928
// Name: Raquel Meira
//
// Date: 02/12/2024
//

#include "imageBW.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instrumentation.h"

// The data structure
//
// A BW image is stored in a structure containing 3 fields:
// Two integers store the image width and height.
// The other field is a pointer to an array that stores the pointers
// to the RLE compressed image rows.
//
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// Constant value --- Use them throughout your code
// const uint8 BLACK = 1;  // Black pixel value, defined on .h
// const uint8 WHITE = 0;  // White pixel value, defined on .h
const int EOR = -1;  // Stored as the last element of a RLE row

// Internal structure for storing RLE BW images
struct image {
  uint32 width;
  uint32 height;
  int** row;  // pointer to an array of pointers referencing the compressed rows
};

// This module follows "design-by-contract" principles.
// Read `Design-by-Contract.md` for more details.

/// Error handling functions

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
//
// When one of these functions fails, it signals this by returning an error
// value such as NULL or 0 (see function documentation), and sets an internal
// variable (errCause) to a string indicating the failure cause.
// The errno global variable thoroughly used in the standard library is
// carefully preserved and propagated, and clients can use it together with
// the ImageErrMsg() function to produce informative error messages.
// The use of the GNU standard library error() function is recommended for
// this purpose.
//
// Additional information:  man 3 errno;  man 3 error;

// Variable to preserve errno temporarily
static int errsave = 0;

// Error cause
static char* errCause;

/// Error cause.
/// After some other module function fails (and returns an error code),
/// calling this function retrieves an appropriate message describing the
/// failure cause.  This may be used together with global variable errno
/// to produce informative error messages (using error(), for instance).
///
/// After a successful operation, the result is not garanteed (it might be
/// the previous error cause).  It is not meant to be used in that situation!
char* ImageErrMsg() {  ///
  return errCause;
}

// Defensive programming aids
//
// Proper defensive programming in C, which lacks an exception mechanism,
// generally leads to possibly long chains of function calls, error checking,
// cleanup code, and return statements:
//   if ( funA(x) == errorA ) { return errorX; }
//   if ( funB(x) == errorB ) { cleanupForA(); return errorY; }
//   if ( funC(x) == errorC ) { cleanupForB(); cleanupForA(); return errorZ; }
//
// Understanding such chains is difficult, and writing them is boring, messy
// and error-prone.  Programmers tend to overlook the intricate details,
// and end up producing unsafe and sometimes incorrect programs.
//
// In this module, we try to deal with these chains using a somewhat
// unorthodox technique.  It resorts to a very simple internal function
// (check) that is used to wrap the function calls and error tests, and chain
// them into a long Boolean expression that reflects the success of the entire
// operation:
//   success =
//   check( funA(x) != error , "MsgFailA" ) &&
//   check( funB(x) != error , "MsgFailB" ) &&
//   check( funC(x) != error , "MsgFailC" ) ;
//   if (!success) {
//     conditionalCleanupCode();
//   }
//   return success;
//
// When a function fails, the chain is interrupted, thanks to the
// short-circuit && operator, and execution jumps to the cleanup code.
// Meanwhile, check() set errCause to an appropriate message.
//
// This technique has some legibility issues and is not always applicable,
// but it is quite concise, and concentrates cleanup code in a single place.
//
// See example utilization in ImageLoad and ImageSave.
//
// (You are not required to use this in your code!)

// Check a condition and set errCause to failmsg in case of failure.
// This may be used to chain a sequence of operations and verify its success.
// Propagates the condition.
// Preserves global errno!
static int check(int condition, const char* failmsg) {
  errCause = (char*)(condition ? "" : failmsg);
  return condition;
}

/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) {  ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Add more macros here...

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!

/// Auxiliary (static) functions

/// Create the header of an image data structure
/// And allocate the array of pointers to RLE rows
static Image AllocateImageHeader(uint32 width, uint32 height) {
  assert(width > 0 && height > 0);
  Image newHeader = malloc(sizeof(struct image));
  assert(newHeader != NULL);

  newHeader->width = width;
  newHeader->height = height;

  // Allocating the array of pointers to RLE rows
  newHeader->row = malloc(height * sizeof(int*));
  assert(newHeader->row != NULL);

  return newHeader;
}

/// Allocate an array to store a RLE row with n elements
static int* AllocateRLERowArray(uint32 n) {
  assert(n > 2);
  int* newArray = malloc(n * sizeof(int));
  assert(newArray != NULL);

  return newArray;
}

/// Conpute the number of runs of a non-compressed (RAW) image row
static uint32 GetNumRunsInRAWRow(uint32 image_width, const uint8* RAW_row) {
  assert(image_width > 0);
  assert(RAW_row != NULL);

  // How many runs?
  uint32 num_runs = 1;
  for (uint32 i = 1; i < image_width; i++) {
    if (RAW_row[i] != RAW_row[i - 1]) {
      num_runs++;
    }
  }

  return num_runs;
}

/// Get the number of runs of a compressed RLE image row
static uint32 GetNumRunsInRLERow(const int* RLE_row) {
  assert(RLE_row != NULL);

  // Go through the RLE_row until EOR is found
  // Discard RLE_row[0], since it is a pixel color

  uint32 num_runs = 0;
  uint32 i = 1;
  while (RLE_row[i] != EOR) {
    num_runs++;
    i++;
  }

  return num_runs;
}

/// Get the number of elementsof an array storing a compressed RLE image row
static uint32 GetSizeRLERowArray(const int* RLE_row) {
  assert(RLE_row != NULL);

  // Go through the array until EOR is found
  uint32 i = 0;
  while (RLE_row[i] != EOR) {
    i++;
  }

  return (i + 1);
}

/// Compress into RLE format a RAW image row
/// Allocates and returns the array storing the image row in RLE format
static int* CompressRow(uint32 image_width, const uint8* RAW_row) {
  assert(image_width > 0);
  assert(RAW_row != NULL);

  // How many runs?
  uint32 num_runs = GetNumRunsInRAWRow(image_width, RAW_row);

  // Allocate the RLE row array
  int* RLE_row = malloc((num_runs + 2) * sizeof(int));
  assert(RLE_row != NULL);

  // Go through the RAW_row
  RLE_row[0] = (int)RAW_row[0];  // Initial pixel value
  uint32 index = 1;
  int num_pixels = 1;
  for (uint32 i = 1; i < image_width; i++) {
    if (RAW_row[i] != RAW_row[i - 1]) {
      RLE_row[index++] = num_pixels;
      num_pixels = 0;
    }
    num_pixels++;
  }
  RLE_row[index++] = num_pixels;
  RLE_row[index] = EOR;  // Reached the end of the row

  return RLE_row;
}

static uint8* UncompressRow(uint32 image_width, const int* RLE_row) {
  assert(image_width > 0);
  assert(RLE_row != NULL);

  // The uncompressed row
  uint8* row = (uint8*)malloc(image_width * sizeof(uint8));
  assert(row != NULL);

  // Go through the RLE_row until EOR is found
  int pixel_value = RLE_row[0];
  uint32 i = 1;
  uint32 dest_i = 0;
  while (RLE_row[i] != EOR) {
    // For each run
    for (int aux = 0; aux < RLE_row[i]; aux++) {
      row[dest_i++] = (uint8)pixel_value;
    }
    // Next run
    i++;
    pixel_value ^= 1;
  }

  return row;
}

// Add your auxiliary functions here...

/// Image management functions

/// Create a new BW image, either BLACK or WHITE.
///   width, height : the dimensions of the new image.
///   val: the pixel color (BLACK or WHITE).
/// Requires: width and height must be non-negative, val is either BLACK or
/// WHITE.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageCreate(uint32 width, uint32 height, uint8 val) {
  assert(width > 0 && height > 0);
  assert(val == WHITE || val == BLACK);

  Image newImage = AllocateImageHeader(width, height);

  // All image pixels have the same value
  int pixel_value = (int)val;

  // Creating the image rows, each row has just 1 run of pixels
  // Each row is represented by an array of 3 elements [value,length,EOR]
  for (uint32 i = 0; i < height; i++) {
    newImage->row[i] = AllocateRLERowArray(3);
    newImage->row[i][0] = pixel_value;
    newImage->row[i][1] = (int)width;
    newImage->row[i][2] = EOR;
  }

  return newImage;
}

/// Create a new BW image, with a perfect CHESSBOARD pattern.
///   width, height : the dimensions of the new image.
///   square_edge : the lenght of the edges of the sqares making up the
///   chessboard pattern.
///   first_value: the pixel color (BLACK or WHITE) of the
///   first image pixel.
/// Requires: width and height must be non-negative, val is either BLACK or
/// WHITE.
/// Requires: for the squares, width and height must be multiples of the
/// edge lenght of the squares
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageCreateChessboard(uint32 width, uint32 height, uint32 square_edge,
                            uint8 first_value) {
  // COMPLETE THE CODE
  // ...
  assert(width > 0 && height > 0);                                                   // Verifica se o width e o height não são negativos
  assert(first_value == WHITE || first_value == BLACK);                              // Se o first_value é white ou black
  assert(width%square_edge == 0 && height%square_edge == 0);                         // Verifica se o width e o heigt são múltiplos do tamanho de um lado do quadrado            

  Image newImageChessboard = AllocateImageHeader(width, height);                     // Aloca o cabeçalho da imagem

  int first_pixel = (int)first_value;
  uint32 num = width/square_edge;                                                    // Números de quadrados existentes

  for(uint32 i = 0; i < height; i++){                                                // Percorre as linhas 
    newImageChessboard->row[i] = AllocateRLERowArray((int)num + 2);                  // Cada linha da imagem que vai ser criada terá alocado em um array o número de quadrados + first_pixel + EOR

    if (i%square_edge == 0 && i != 0) {                                              // Para o 1º elemento de cada linha muda a cor entre WHITE e BLACK alternadamente
      first_pixel = first_pixel == BLACK ? WHITE : BLACK;
    }

    newImageChessboard->row[i][0] = first_pixel;                                     // O 1º elemento de cada linha vai ter o valor do first_pixel

    for (uint32 j = 0; j < num; j++) {                                               // Para cada quadrado possível
      newImageChessboard->row[i][j+1] = (int)square_edge;                            // Os próximos elementos vão ter a largura do quadrado
    }

    newImageChessboard->row[i][(int)num + 1] = EOR;                                  // O último elemento da linha será o end of row (EOR)
  }

  return newImageChessboard;
}

/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
/// Ensures: (*imgp)==NULL.
/// Should never fail.
void ImageDestroy(Image* imgp) {
  assert(imgp != NULL);

  Image img = *imgp;

  for (uint32 i = 0; i < img->height; i++) {
    free(img->row[i]);
  }
  free(img->row);
  free(img);

  *imgp = NULL;
}

/// Printing on the console

/// Output the raw BW image
void ImageRAWPrint(const Image img) {
  assert(img != NULL);

  printf("width = %d height = %d\n", img->width, img->height);
  printf(" RAW image\n");

  // Print the pixels of each image row
  for (uint32 i = 0; i < img->height; i++) {
    // The value of the first pixel in the current row
    int pixel_value = img->row[i][0];
    for (uint32 j = 1; img->row[i][j] != EOR; j++) {
      // Print the current run of pixels
      for (int k = 0; k < img->row[i][j]; k++) {
        printf("%d", pixel_value);
      }
      // Switch (XOR) to the pixel value for the next run, if any
      pixel_value ^= 1;
    }
    // At current row end
    printf("\n");
  }
  printf("\n");
}

/// Output the compressed RLE image
void ImageRLEPrint(const Image img) {
  assert(img != NULL);

  printf("width = %d height = %d\n", img->width, img->height);
  printf(" RLE encoding\n");

  // Print the compressed rows information
  for (uint32 i = 0; i < img->height; i++) {
    uint32 j;
    for (j = 0; img->row[i][j] != EOR; j++) {
      printf("%d ", img->row[i][j]);
    }
    printf("%d\n", img->row[i][j]);
  }
  printf("\n");
}

/// PBM BW file operations

// See PBM format specification: http://netpbm.sourceforge.net/doc/pbm.html

// Auxiliary function
static void unpackBits(int nbytes, const uint8 bytes[], uint8 raw_row[]) {
  // bitmask starts at top bit
  int offset = 0;
  uint8 mask = 1 << (7 - offset);
  while (offset < 8) {  // or (mask > 0)
    for (int b = 0; b < nbytes; b++) {
      raw_row[8 * b + offset] = (bytes[b] & mask) != 0;
    }
    mask >>= 1;
    offset++;
  }
}

// Auxiliary function
static void packBits(int nbytes, uint8 bytes[], const uint8 raw_row[]) {
  // bitmask starts at top bit
  int offset = 0;
  uint8 mask = 1 << (7 - offset);
  while (offset < 8) {  // or (mask > 0)
    for (int b = 0; b < nbytes; b++) {
      if (offset == 0) bytes[b] = 0;
      bytes[b] |= raw_row[8 * b + offset] ? mask : 0;
    }
    mask >>= 1;
    offset++;
  }
}

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PBM file.
/// Only binary PBM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageLoad(const char* filename) {  ///
  int w, h;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  check((f = fopen(filename, "rb")) != NULL, "Open failed");
  // Parse PBM header
  check(fscanf(f, "P%c ", &c) == 1 && c == '4', "Invalid file format");
  skipComments(f);
  check(fscanf(f, "%d ", &w) == 1 && w >= 0, "Invalid width");
  skipComments(f);
  check(fscanf(f, "%d", &h) == 1 && h >= 0, "Invalid height");
  check(fscanf(f, "%c", &c) == 1 && isspace(c), "Whitespace expected");

  // Allocate image
  img = AllocateImageHeader(w, h);

  // Read pixels
  int nbytes = (w + 8 - 1) / 8;  // number of bytes for each row
  // using VLAs...
  uint8 bytes[nbytes];
  uint8 raw_row[nbytes * 8];
  for (uint32 i = 0; i < img->height; i++) {
    check(fread(bytes, sizeof(uint8), nbytes, f) == (size_t)nbytes,
          "Reading pixels");
    unpackBits(nbytes, bytes, raw_row);
    img->row[i] = CompressRow(w, raw_row);
  }

  fclose(f);
  return img;
}

/// Save image to PBM file.
/// On success, returns nonzero.
/// On failure, returns 0, and
/// a partial and invalid file may be left in the system.
int ImageSave(const Image img, const char* filename) {  ///
  assert(img != NULL);
  int w = img->width;
  int h = img->height;
  FILE* f = NULL;

  check((f = fopen(filename, "wb")) != NULL, "Open failed");
  check(fprintf(f, "P4\n%d %d\n", w, h) > 0, "Writing header failed");

  // Write pixels
  int nbytes = (w + 8 - 1) / 8;  // number of bytes for each row
  // using VLAs...
  uint8 bytes[nbytes];
  // unit8 raw_row[nbytes*8];
  for (uint32 i = 0; i < img->height; i++) {
    // UncompressRow...
    uint8* raw_row = UncompressRow(nbytes * 8, img->row[i]);
    // Fill padding pixels with WHITE
    memset(raw_row + w, WHITE, nbytes * 8 - w);
    packBits(nbytes, bytes, raw_row);
    check(fwrite(bytes, sizeof(uint8), nbytes, f) == (size_t)nbytes,
          "Writing pixels failed");
    free(raw_row);
  }

  // Cleanup
  fclose(f);
  return 0;
}

/// Information queries

/// Get image width
int ImageWidth(const Image img) {
  assert(img != NULL);
  return img->width;
}

/// Get image height
int ImageHeight(const Image img) {
  assert(img != NULL);
  return img->height;
}

/// Image comparison

int ImageIsEqual(const Image img1, const Image img2) {
  assert(img1 != NULL && img2 != NULL);

  // COMPLETE THE CODE
  uint32 width1 = img1->width;    // Largura da 1ª imagem
  uint32 height1 = img1->height;  // Altura da 1ª imagem

  uint32 witdh2 = img2->width;    // Largura da 2ª imagem
  uint32 height2 = img2->height;   // Altura da 2ª imagem

  int** rows1 = img1->row;          // Ponteiro para as linhas da 1ª imagem
  int** rows2 = img2->row;          // Ponteiro para as linhas da 2ª imagem                                  
  
  for(uint32 i = 0; i < height1; i++){                            // Para cada linha da imagem
    uint8* row1 = UncompressRow(width1, rows1[i]);                 // Passa de RLE para RAW
    uint8* row2 = UncompressRow(witdh2, rows2[i]);                  

    for(uint32 j = 0; j < width1; j++){                            // Para cada pixel da linha
      if(row1[j] != row2[j]){                                      // Se os pixels forem diferentes
        return 0;                                                  // Retorna 0
      }
    }
  free(row1);
  free(row2);
  }
  return 1;
}

int ImageIsDifferent(const Image img1, const Image img2) {
  assert(img1 != NULL && img2 != NULL);
  return !ImageIsEqual(img1, img2);
}

/// Boolean Operations on image pixels

/// These functions apply boolean operations to images,
/// returning a new image as a result.
///
/// Operand images are left untouched and must be of the same size.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)

Image ImageNEG(const Image img) {
  assert(img != NULL);

  uint32 width = img->width;
  uint32 height = img->height;

  Image newImage = AllocateImageHeader(width, height);

  // Directly copying the rows, one by one
  // And changing the value of row[i][0]

  for (uint32 i = 0; i < height; i++) {
    uint32 num_elems = GetSizeRLERowArray(img->row[i]);
    newImage->row[i] = AllocateRLERowArray(num_elems);
    memcpy(newImage->row[i], img->row[i], num_elems * sizeof(int));
    newImage->row[i][0] ^= 1;  // Just negate the value of the first pixel run
  }

  return newImage;
}

// Image ImageAND(const Image img1, const Image img2) {
//   assert(img1 != NULL && img2 != NULL);                                               // Verifica se elas existem
//   assert(img1->width == img2->width && img1->height == img2->height);                 // Verifica se tem ambas a mesma altura e largura
//   // COMPLETE THE CODE
//   // You might consider using the UncompressRow and CompressRow auxiliary files
//   // Or devise a more efficient alternative
//   // ...

//   InstrReset();
//   InstrName[0] = "Opps";

//   uint32 width = img1->width;                                                         
//   uint32 height = img1->height;
//   Image newImageAND = AllocateImageHeader(width,height);                              

//   for(uint32 i = 0; i < height; i++){
//     uint8* row_img1 = UncompressRow(width,img1->row[i]);                              // Cada linha da 1ª imagem vai passar de RLE para RAW
//     uint8* row_img2 = UncompressRow(width,img2->row[i]);

//     uint8* newImagerow = malloc(width * sizeof(uint8));                               // A nova imagem vai guardar o tamnho da largura que precisa de ter
//     for(uint32 j = 0; j < width; j++){
//       newImagerow[j] = row_img1[j] & row_img2[j];                                     // A cada linha das duas imagens foi aplicada a operação de AND 
//       InstrCount[0] += 1;
//     }

//     newImageAND->row[i] = CompressRow(width, newImagerow);                            // Passa de RAW para RLE
//     free(row_img1);                            
//     free(row_img2);
//   }  

//   InstrPrint();

//   return newImageAND;
// }


Image ImageAND(const Image img1, const Image img2) {
    assert(img1 != NULL && img2 != NULL);                                                           // Verifica se as imagens não são nulas e têm as mesmas dimensões
    assert(ImageWidth(img1) == ImageWidth(img2) && ImageHeight(img1) == ImageHeight(img2));
    
    InstrReset();                                                                                   // Reset dos contadores de operações
    InstrName[0] = "Opps";

    int height = ImageHeight(img1);
    int width = ImageWidth(img1);
    
    Image result = AllocateImageHeader(width, height);                                              // Aloca uma nova imagem para armazenar o resultado
    assert(result != NULL);

    for (uint32 y = 0; y < height; y++) {                                                           // Itera sobre cada linha da imagem     
        const int* rle_row1 = img1->row[y];                                                         // Ponteiro que aponta para cada linha da imagem 1
        const int* rle_row2 = img2->row[y];
        int* rle_result = AllocateRLERowArray(width);

        int idx1 = 1, idx2 = 1;                                                                     // Começa no 1, pois o 0 é o first pixel
        int index_res = 0;                                                                          // Index do retorno
        int sum_0s = 0;                                                                             // Run de cor branca (0s)
        int val1 = rle_row1[0];                                                                     // Cor da run atual da imagem 1
        int val2 = rle_row2[0];
        int rest1 = 0, rest2 = 0;                                                                   // Resto da run atual                                                                        

        rle_result[index_res++] = val1 == val2 && val1 == BLACK ? BLACK : WHITE;                    // Se ambos as primeiras runs são BLACK então a 1 run da nova imagem é BLACK

        while (rle_row1[idx1] != EOR && rle_row2[idx2] != EOR) {                                    // While até pelo menos uma das linhas terminar as runs
            
            int val_row1 = rest1 == 0 ? rle_row1[idx1] : rest1;                                     // Utiliza o resto antes de utilizar o valor da próxima run
            int val_row2 = rest2 == 0 ? rle_row2[idx2] : rest2;                                     
            int min_val = (val_row1 < val_row2) ? val_row1 : val_row2;                              // Valor minimo

            if (val1 == val2 && val1 == BLACK) {                                                    // Se ambas as runs forem BLACK

              if (sum_0s != 0) {
                rle_result[index_res++] = sum_0s;                                                   // Tamanho de uma run de WHITE (0)
                sum_0s = 0;
              }

              rle_result[index_res++] = min_val;                                                    // Guarda o valor da run

            } else {                                                                                // Se uma run for da cor WHITE
              sum_0s += min_val;                                                                    // Soma do tamanho minimo
              rest1 = 0; rest2 = 0;
            }

            if (val_row1 > min_val) {                                                               // Se for maior que o valor guardado
              rest1 = val_row1 - min_val;                                                           // A esse valor subtrai-se o valor minimo
            } else {
              val1 = val1 == BLACK ? WHITE : BLACK;                                                 // Senão guarda-se que a cor da próxima run é diferente
              idx1++;                                                                               // Passar para a próxima run
            }

            if (val_row2 > min_val) {
              rest2 = val_row2 - min_val;
            } else {
              val2 = val2 == BLACK ? WHITE : BLACK;
              idx2++;
            }

            InstrCount[0]++;                                                                        // Incrementar a operação
        }

        if (sum_0s != 0) {
          rle_result[index_res++] = sum_0s;                                                         // Guardar no final os 0s
          sum_0s = 0;
        }

        rle_result[index_res] = EOR;                                                                // Adicionar a linha da nova imagem o EOR
        result->row[y] = rle_result;                                                                // Adicionar a linha a imagem
    }

    InstrPrint();

    return result;
}

Image ImageOR(const Image img1, const Image img2) {
  assert(img1 != NULL && img2 != NULL);
  assert(img1->width == img2->width && img1->height == img2->height);
  // COMPLETE THE CODE
  // You might consider using the UncompressRow and CompressRow auxiliary files
  // Or devise a more efficient alternative
  // ...

  uint32 width = img1->width;
  uint32 height = img1->height;
  Image newImageOR = AllocateImageHeader(width,height);

  for(uint32 i = 0; i < height; i++){
    uint8* row_img1 = UncompressRow(width,img1->row[i]);                              // Cada linha da 1ª imagem vai passar de RLE para RAW  
    uint8* row_img2 = UncompressRow(width,img2->row[i]);

    uint8* newImagerow = malloc(width * sizeof(uint8));                               // A nova imagem vai guardar o tamnho da largura que precisa de ter
    for(uint32 j = 0; j < width; j++){
      newImagerow[j] = row_img1[j] | row_img2[j];                                     // A cada linha das duas imagens foi aplicada a operação de OR 
    }
    newImageOR->row[i] = CompressRow(width, newImagerow);                             // Passa de RAW para RLE
    free(row_img1);                            
    free(row_img2);
  }  
  return newImageOR;
}

Image ImageXOR(Image img1, Image img2) {
  assert(img1 != NULL && img2 != NULL);
  assert(img1->width == img2->width && img1->height == img2->height);
  // COMPLETE THE CODE
  // You might consider using the UncompressRow and CompressRow auxiliary files
  // Or devise a more efficient alternative
  // ...

  uint32 width = img1->width;
  uint32 height = img1->height;
  Image newImageXOR = AllocateImageHeader(width,height);

  for(uint32 i = 0; i < height; i++){
    uint8* row_img1 = UncompressRow(width,img1->row[i]);                              // Cada linha da 1ª imagem vai passar de RLE para RAW
    uint8* row_img2 = UncompressRow(width,img2->row[i]);

    uint8* newImagerow = malloc(width * sizeof(uint8));                               // A nova imagem vai guardar o tamnho da largura que precisa de ter
    for(uint32 j = 0; j < width; j++){
      newImagerow[j] = row_img1[j] ^ row_img2[j];                                     // A cada linha das duas imagens foi aplicada a operação de XOR 
    }
    newImageXOR->row[i] = CompressRow(width, newImagerow);                            // Passa de RAW para RLE
    free(row_img1);                            
    free(row_img2);
  }  
  return newImageXOR;
}

/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)

/// Mirror an image = flip top-bottom.
/// Returns a mirrored version of the image.
/// Ensures: The original img is not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageHorizontalMirror(const Image img) {
  assert(img != NULL);

  uint32 width = img->width;
  uint32 height = img->height;

  Image newImageHMirror = AllocateImageHeader(width, height);

  // COMPLETE THE CODE
  // ...

  for(uint32 i = 0; i < height; i++){                                                 // Vai percorrer todas as linhas                                                  
    uint32 inverted = height - i - 1;                                                 // Calcula o índice da linha correspondente no espelho horizontal. 
    newImageHMirror->row[i] = img->row[inverted];                                     // A primeira linha da nova imagem será a última da original, a segunda será a penúltima, e assim sucessivamente.
  }
  return newImageHMirror;
}

/// Mirror an image = flip left-right.
/// Returns a mirrored version of the image.
/// Ensures: The original img is not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageVerticalMirror(const Image img) {
  assert(img != NULL);

  uint32 width = img->width;
  uint32 height = img->height;

  Image newImageVMirror = AllocateImageHeader(width, height);

  // COMPLETE THE CODE
  // ...

  for(uint32 i = 0; i < height; i++){
    uint8* row = UncompressRow(width,img->row[i]);                                    // Descomprime a linha atual da imagem original
    uint8* newImagerow = malloc(width * sizeof(uint8));

    for(uint32 j = 0; j < width; j++){
      uint32 inverted = width - j - 1;                                                // Calcula o índice da linha correspondente no espelho vertical.
      newImagerow[j] = row[inverted];                                                 // Copia o valor do pixel invertido para a nova linha.
    }
    newImageVMirror->row[i] = CompressRow(width, newImagerow);                        // Comprime a nova linha  
    free(row);
  }
  return newImageVMirror;
}

/// Replicate img2 at the bottom of imag1, creating a larger image
/// Requires: the width of the two images must be the same.
/// Returns the new larger image.
/// Ensures: The original images are not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageReplicateAtBottom(const Image img1, const Image img2) {
    assert(img1 != NULL && img2 != NULL);
    assert(img1->width == img2->width);

    uint32 new_width = img1->width;
    uint32 new_height = img1->height + img2->height;

    Image newImage = AllocateImageHeader(new_width, new_height);

                                                                    
    for (uint32 i = 0; i < new_height; i++) {                                         // Copia as rows da imagem 1 para a nova imagem
      uint8* newIamgerow;

      if(i < img1->height){                                                           // Copia as rows da imagem 1
        newIamgerow = UncompressRow(new_width, img1->row[i]);    
      } else {                                                                        // Quando acaba de copiar as rows da imagem 1, copia as rows da imagem 2
        newIamgerow = UncompressRow(new_width, img2->row[i - img1->height]);   
      }

      newImage->row[i] = CompressRow(new_width, newIamgerow);                         // Comprime as rows da nova imagem
    }
    

    return newImage;
}



/// Replicate img2 to the right of imag1, creating a larger image
/// Requires: the height of the two images must be the same.
/// Returns the new larger image.
/// Ensures: The original images are not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageReplicateAtRight(const Image img1, const Image img2) {
  assert(img1 != NULL && img2 != NULL);
  assert(img1->height == img2->height);

  uint32 new_width = img1->width + img2->width;
  uint32 new_height = img1->height;

  Image newImage = AllocateImageHeader(new_width, new_height);
                                                                           
  for (uint32 i = 0; i < new_height; i++) {                                 // Processa cada linha das imagens para criar as linhas da nova imagem

      uint8* newIamgerow;
                                                                            // Ponteiros para as linhas descompactadas das imagens originais
      uint8* row1 = UncompressRow(img1->width, img1->row[i]);
      uint8* row2 = UncompressRow(img2->width, img2->row[i]);
                                                                            // Aloca espaço para a nova linha que terá a largura combinada das duas imagens
      newIamgerow = malloc(new_width * sizeof(uint8));
      
      for (uint32 j = 0; j < new_width; j++) {
        if (j < img1->width) {                                               // Copia as rows da imagem 1
          newIamgerow[j] = row1[j];
        } else {                                                             // Quando acaba de copiar as rows da imagem 1, copia as rows da imagem 2
          newIamgerow[j] = row2[j - img1->width];
        }
      }                                                                      
      newImage->row[i] = CompressRow(new_width, newIamgerow);                // Comprime as rows da nova imagem
    }
  return newImage;
}
