/* Sequential implementation of the Game of life.
 * Use gcc to compile.
 * Usage ./seqlife --filename <filename> --gens <gens> --rows <rows> --cols <cols> --list <list>
 */

#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<string.h>
#include<mpi.h>

#define DEFAULT_GENS 100
#define DEFAULT_ROWS 100
#define DEFAULT_COLS 100
#define DEFAULT_FILE "inputFile"
#define MAX_LIST_LEN 100

void gameOfLife(char *fileName, int gens, int rows, int cols, int* genList, int listLen, int myRank, int P);
void printTheBoard(int *gameBoard, int rows, int cols);
int compareInt(const void *a, const void *b);

int main(int argc, char* argv[]) {


  MPI_Init(&argc, &argv);
  int myRank; 
  int P;
    
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &P);
  
  char *fileName = malloc(sizeof(char)*30); 
  strcpy(fileName, DEFAULT_FILE);
  int gens = DEFAULT_GENS;
  int rows = DEFAULT_ROWS;
  int cols = DEFAULT_COLS;
  
  int* genList;
  genList = malloc(sizeof(int)*MAX_LIST_LEN);
  int listLen = 0;
  int i = 1;
  //parsing the args
  while(i < argc) {
    if(strcmp(argv[i], "--filename")==0 && i+1 < argc) {
      //set filename
      fileName = argv[++i];
    } else if(strcmp(argv[i], "--gens")==0 && i+1 < argc) {
      //set gens
      gens = atoi(argv[++i]);
    } else if(strcmp(argv[i], "--rows")==0 && i+1 < argc) {
      //set rows
      rows = atoi(argv[++i]);
    } else if(strcmp(argv[i], "--cols")==0 && i+1 < argc) {
      //set cols
      cols = atoi(argv[++i]);
    } else if(strcmp(argv[i], "--list")==0 && i+1 < argc) {
      //set list
      while(i+1 < argc && listLen < MAX_LIST_LEN) {
        //printf("%s\n",argv[i+1]); fflush(stdout);
        if(isdigit(argv[i+1][0])) {
	  //printf("adding: "); fflush(stdout);
	  genList[listLen++] = atoi(argv[++i]);
	  //printf("added %s\n", argv[i]); fflush(stdout);
	}
      }
      qsort(genList, listLen, sizeof(int), compareInt); //Make them in order.
    } else {
      printf("Error at \"%s\"\n", argv[i]);
      return -1;
    }
    i++;
  } 
  MPI_Barrier(MPI_COMM_WORLD);
  
  if(myRank == 0) {
    printf("fileName: %s\n", fileName);
    printf("gens: %d\n", gens);
    printf("rows: %d\n", rows);
    printf("cols: %d\n", cols);
    printf("listLen: %d\n", listLen);
    fflush(stdout);
  }

  gameOfLife(fileName, gens, rows, cols, genList, listLen, myRank, P);
  
  free(genList);
  MPI_Finalize();
  return 0;
}

/* Game of Life algorithm. */
void gameOfLife(char *fileName, int gens, int rows, int cols, int* genList, int listLen, int myRank, int P) {
  int* gameBoard = malloc(sizeof(int)*rows*cols); //This is the game board. For the sequential algorithm, we are using straight rowsxcols integer array. This will be compactified a bit in the parallel version. Hopefully.
  int* nextGen = malloc(sizeof(int)*rows*cols);

  //printf("hello world from %d of %d\n", myRank, P);
  
  int i = 0; int j = 0;
  //master read in to the gameboard
  if (myRank == 0) {
    FILE *inFile = fopen(fileName, "r");
    if(inFile == NULL) { printf("nope. File not found.\n"); return; }
    int max_size = rows*cols;
    int c = fgetc(inFile);
    //printf("reading the file\n"); fflush(stdout);
    while( c != EOF && i < max_size) {
      if(isdigit(c)) {
        gameBoard[i++] = c - '0'; //That's right.
        fflush(stdout);
      }
      c = fgetc(inFile);
    }
    fclose(inFile);
  }

  
  // Timer start-o
  double startTime, endTime;
  startTime = MPI_Wtime();
  
  MPI_Bcast(gameBoard, rows*cols, MPI_INT, 0, MPI_COMM_WORLD);

  int gen = 0; int listCounter = 0;
  int neighborCount = 0;
  for(gen = 0; gen <= gens; gen++) {
    
    //If current gen is for printing, then print.
    if (myRank == 0) {
      if(listCounter < listLen && gen == genList[listCounter]) {
        printf("gen: %d\n", gen); fflush(stdout);
        printTheBoard(gameBoard, rows, cols);
        listCounter++;
      }
    }
    for(i = myRank*rows/P; i < (myRank+1)*rows/P; i++) {
      for(j = 0; j < cols; j++) {
        int live = gameBoard[i*cols + j];
	neighborCount = 0;
	neighborCount += gameBoard[(i == 0 ? rows-1 : i-1)*cols + (j == 0 ? cols-1 : j-1)];
	neighborCount += gameBoard[(i == 0 ? rows-1 : i-1)*cols + j];
	neighborCount += gameBoard[(i == 0 ? rows-1 : i-1)*cols + (j+1)%cols];
	neighborCount += gameBoard[i*cols + (j == 0 ? cols-1 : j-1)];
	neighborCount += gameBoard[i*cols + (j+1)%cols];
	neighborCount += gameBoard[(i+1)%rows*cols + (j == 0 ? cols-1 : j-1)];
	neighborCount += gameBoard[(i+1)%rows*cols + j];
	neighborCount += gameBoard[(i+1)%rows*cols + (j+1)%cols];
	//if (myRank == 0) { printf("(%d,%d): live: %d, neighbors: %d\n",i,j,live,neighborCount); fflush(stdout); }
	if(live) {
	  if(neighborCount < 2 || neighborCount >3) {
	    nextGen[i*cols + j] = 0;
	  } else nextGen[i*cols +j] = 1;
	} else {
	  if(neighborCount == 3) {
	    nextGen[i*cols+j] = 1;
	  } else {
	    nextGen[i*cols+j] = 0;
	  }
	}
      }
    }
    
    MPI_Allgather(&nextGen[(myRank*rows*cols)/P],rows*cols/P, 
                  MPI_INT, gameBoard, rows*cols/P,
		  MPI_INT, MPI_COMM_WORLD); //Everybody gets all the things!

    //clock_t goal = 1000*(CLOCKS_PER_SEC/1000) + clock();
    //while (goal > clock()) gen = gen;
    //temp = nextGen;
    //nextGen = gameBoard;
    //gameBoard = temp;
  }

  endTime = MPI_Wtime();

  double timeDiff = endTime - startTime;
  for(i = 0; i < P; i ++) {
    if (myRank == i) printf("Processor %d: %f seconds\n", myRank, timeDiff); fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
  }
  
  free(gameBoard);
  free(nextGen);
}

void printTheBoard(int *gameBoard, int rows, int cols) {
  int i = 0; int j = 0;
  for(i = 0; i < cols; i++) {
    //printf("Row %d: ", i);
    for(j = 0; j < rows; j++) {
      //printf("%c ", gameBoard[i*cols+j] == 1 ? 'x' : ' ');
      printf("%d ", gameBoard[i*cols+j]);
      fflush(stdout);
    }
    printf("\n"); fflush(stdout);
  }
}

/* comparator for the qsort function.
 */
int compareInt(const void *a, const void *b) {
  int x = *(int *)a;
  int y = *(int *)b;
  if ( x < y) { return -1; }
  if ( x > y) { return 1; }
  return 0;
}
