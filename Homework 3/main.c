//
//  main.c
//  Homework 3
//
//  Created by Adam on 3/22/19.
//  Copyright © 2019 Adam Kuniholm. All rights reserved.
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <limits.h> 


#define ACTIVE -3
#define EMPTY -1
#define INVALID -2


/*
 use C and the POSIX thread (Pthread) library to implement a
 single-process multi-threaded system 

 solve the knight’s tour problem 
 
 i.e., can a knight move over all squares exactly once on a given board?
 
 global variable: max_squares 
    - maximum number of squares covered by Sonny
    - update with new maximums
 
 global variable: dead_end_boards
    - list of dead end board configurations
   +----------------------------------> n
   | .--------..--------..--------.  
   | |.------.||        ||        |  
   | ||+0||+0|||  --->  || --.    |  
   | |`------`||        ||   v    |  
   | `--------``--------``--------`
   |                     .--------.
   |                     |.------.|
   |                     ||+2||+1||
   |                     |`------`|
   |                     `--------`
   |
   v
   m
 Valid Move:
    - Sonny the knight moves two squares in direction D 
        and then one square and then one square 90deg from D
        where D { up, down, right, left}
                      * up    : (i+1, j-2) *
          (i-2, j+1)<-                      ->(i+2, j+1)
                      * down  : (i+1, j+2) *
    - Sonny may not land on a square more than once in his tour
 
 Sonny starts in the upper-left corner of the board. (0,0)
  m = rows, n columns, x indicates that the main thread should
                         display all “dead end” boards with at 
                         least x squares covered.
 // 1        .------.
 // c-2, r-1 | d    |
 //          | cba  |
 //          `------`
 // 2        .-----.
 // c-1, r-2 | dc  | 
 //          |  b  |
 //          |  a  |
 //          `-----`
 // 3        .-----.   
 // c+1, r-2 |  cd |    
 //          |  b  |    
 //          |  a  |     
 //          `-----` 
 // 4        .------.
 // c+2, r-1 |   d  |
 //          | abc  |
 //          `------`
 // 5        .------.
 // c+2, r+1 | abc  |
 //          |   d  |
 //          `------`
 // 6        .-----.
 // c+1, r+2 |  a  |
 //          |  b  |
 //          |  cd |
 //          `-----`
 // 7        .-----.      
 // c-1, r+2 |  a  |  
 //          |  b  |    
 //          | dc  |
 //          `-----`
 // 8        .------.
 // c-2, r+1 | cba  |
 //          | d    |
 //          `------`
 */



struct Node  {
  char * value;
  int index;
}; typedef struct Node Node;
struct Board {
  int ** data;
  int rows;
  int cols;
  int x;
  int y;
  int moves;
  int depth;
}; typedef struct Board Board;

pthread_mutex_t dead_end_board_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t max_squares_mutex = PTHREAD_MUTEX_INITIALIZER;
const int dx[8] = {-2,-1, 1, 2, 2, 1,-1,-2};
const int dy[8] = {-1,-2,-2,-1, 1, 2, 2, 1};
int max_squares = 0;
Board ** dead_end_boards;
int dead_end_boards_count = 0;
int dead_end_boards_size = 1;

void freeBoard(Board * b) {
  for (int i = 0; i < b->rows; i++) 
    free(b->data[i]);
  free(b->data);
  free(b);
}
Board* makeBoard(const int rows, const int cols) {
  if (rows < 1 || cols < 1)
    return NULL;
  Board* board = calloc(1, sizeof(Board));
  board->rows = rows;
  board->cols = cols;
  board->x = 0;
  board->y = 0;
  board->moves = 0;
  board->depth = 0;
  board->data = calloc(cols, sizeof(int*));
  for (int x = 0; x < cols; x++) {
    board->data[x] = calloc(rows, sizeof(int));
    for (int y = 0; y < rows; y++)
      board->data[x][y] = EMPTY;
  }
  return board;
}

int itemAt(const Board * b, const int x, const int y) {
  if (x < 0 || y < 0 || y >= b->rows || x >= b->cols) 
    return INVALID;
  else return b->data[x][y]; 
}

int itemIs(Board * b, const int x, const int y, int new) {
  int old = itemAt(b, x, y);
  if (old != INVALID) 
    b->data[x][y] = new;
  return old;
}


Board * copyBoard(const Board * b) {
  Board* newB = makeBoard(b->rows, b->cols);
  newB->x = b->x;
  newB->y = b->y;
  newB->depth = b->depth;
  newB->moves = b->moves;
  for (int x = 0; x < b->cols; x++)
    for (int y = 0; y < b->rows; y++)
      itemIs(newB, x, y, itemAt(b, x, y));
  return newB;
}




unsigned long hashBoard(const Board * b) {
  // http://www.cse.yorku.ca/~oz/hash.html
  // https://github.com/jamesroutley/write-a-hash-table/tree/master/03-hashing
  unsigned long hash = 13;  
  for (int x = 0; x < b->cols; x++) {
    for (int y = 0; y < b->rows; y++) {
      const int this = itemAt(b, x, y);
      hash = hash*151 + (this+3);
    }
  }
  return hash;
}

void printLine(int n) {
  for (int i = 0; i < n; i++) 
    printf("+---"); 
  printf("+\n");
}

void printBoardDebug(const Board* b) {
  for (int y = 0; y < b->rows; y++){
    printLine(b->cols);
    for (int x = 0; x < b->cols; x++) {
      int item = itemAt(b, x, y);
      if (item == INVALID) 
        printf("| ! ");
      else if (item == EMPTY)
        printf("|   ");
      else if (item == ACTIVE)
        printf("|<S>");
      else if (item > b->rows * b->cols) 
        printf("| %c ", item);
      else
        printf("| %d ", item);
    }
    printf("|");
    if (y == 0) 
      printf("  %dx%d board\n", b->rows, b->cols);
    else if (y == 1) 
      printf("  %d moves\n", b->moves);
    else if (y == 2)
      printf("  %d deep\n", b->depth);
    else
      printf("\n");
  }
  printLine(b->cols);
  printf("Hash: %lu\n", hashBoard(b));
  fflush(stdout);
}
void printBoard(const Board* b, pthread_t tid) {
  for (int y = 0; y < b->rows; y++){
    char bLine[b->cols+1];
    bLine[b->cols] = '\0';
    for (int x = 0; x < b->cols; x++) {
      int item = itemAt(b, x, y);
      if (item == INVALID) 
        bLine[x] = '!';
      else if (item == EMPTY)
        bLine[x] = '.';
      else 
        bLine[x] = 'S';
    }
    char prefix = y == 0 ? '>' : ' ';
    printf("THREAD %d: %c %s\n", (int)tid, prefix, bLine);
  }
  fflush(stdout);
}

int isFree(const Board * b, const int x, const int y) {
  return itemAt(b, x, y) == EMPTY;
}



//unsigned long makeHash(const nGram* n, const char * str) {
//  // http://www.cse.yorku.ca/~oz/hash.html
//  // https://github.com/jamesroutley/write-a-hash-table/tree/master/03-hashing
//  
//  unsigned long h;
//  unsigned const int *us;
//  
//  /* cast s to unsigned const char * */
//  /* this ensures that elements of s will be treated as having values >= 0 */
//  us = (unsigned const int *) str;
//  
//  h = 0;
//  //int multipliers[] = {151, 157};
//  while(*us != '\0') {
//    h = h * 151 + (*us+3);
//    us++;
//  }
//  h = h%n->size;
//  return h;
//  
//  return h;
//}

int makeMove(Board * b, int xp, int yp) {
  if (!isFree(b, xp, yp)) return 0==1; 
  const int moveOut = itemIs(b, b->x, b->y, b->moves++) == ACTIVE;
  const int moveIn = itemIs(b, xp, yp, ACTIVE) == EMPTY;
  if (moveOut && moveIn) {
    
  } else {
#ifdef DEBUG
    printf("uh oh definitely board some problems\n");
#endif
  }
  b->x = xp;
  b->y = yp;
  return moveOut && moveIn;
}
void undoMove(Board *b, int xo, int yo) {
  itemIs(b, b->x, b->y, EMPTY);
  itemIs(b, xo, yo, ACTIVE);
  b->x = xo;
  b->y = yo;
}
int possibleMoveCount(const Board *b) {
  int moveCount = 0;
  for (int i = 0; i < 8; i++) 
    moveCount += isFree(b, b->x + dy[i], b->y + dx[i]) ? 1 : 0;
  
  return moveCount;
}

void markPossibleMoves(const Board * board) {
  Board * b = copyBoard(board);
  const char a[8] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
  int successes = 0;
  for (int i = 0; i < 8; i++) {
    if (isFree(b, b->x + dx[i], b->y + dy[i])) {
      itemIs(b, b->x + dx[i], b->y + dy[i], a[successes++]);
    }
  }
  printBoardDebug(b);
  freeBoard(b);
}

void addDeadEndBoard(Board * b) {
  pthread_mutex_lock( &dead_end_board_mutex );
#ifdef DEBUG
  printBoardDebug(b);
#endif
  if (b->depth > 0 && dead_end_boards_count >= dead_end_boards_size) {
    dead_end_boards_size  = dead_end_boards_size < 16 ? 16 : dead_end_boards_size*2;
    dead_end_boards = realloc(dead_end_boards, dead_end_boards_size*sizeof(Board*));
  }
  dead_end_boards[dead_end_boards_count++] = copyBoard(b); 
  pthread_mutex_unlock( &dead_end_board_mutex );
}

void reportMoveCount(Board * b) {
  pthread_mutex_lock( &max_squares_mutex );
  max_squares = max_squares < b->moves ? b->moves : max_squares;
  pthread_mutex_unlock( &max_squares_mutex );

}

void * maxSquares(void * argv) {
  Board * b = (Board*) argv;
  
  unsigned int * maxMoves = calloc(1, sizeof(unsigned int));
  *maxMoves = b->moves;
  if (b->moves == b->rows * b->cols) {
    printf("THREAD %d: Sonny found a full knight's tour!\n", (int)pthread_self());
    fflush(stdout);
    reportMoveCount(b);
#ifdef DEBUG
    printBoardDebug(b);
#endif
    pthread_exit(maxMoves);
    return maxMoves; 
  } else if (b->moves > b->rows * b->cols) {
    perror("Impossible number of moves made");
    *maxMoves = b->rows * b->cols;
    pthread_exit(maxMoves);
    return maxMoves; 
  }
  
  // Call threads for up, down, right, left
  int possible = possibleMoveCount(b);
  if (possible > 1)  {
    printf("THREAD %d: %d moves possible after move #%d; creating threads...\n", 
           (int)pthread_self(), possible, b->depth+1);
    fflush(stdout);
#ifdef DEBUG
//    markPossibleMoves(b);
#endif
  } else if (possible == 0) {
    printf("THREAD %d: Dead end after move #%d\n", (int)pthread_self(), b->moves);
    fflush(stdout);
    reportMoveCount(b);
    addDeadEndBoard(b);
#ifdef DEBUG
    printBoardDebug(b);
#endif
    pthread_exit(maxMoves);
    return maxMoves;
  }
  pthread_t tids[possible];
  Board** bs = calloc(possible, sizeof(Board*));
  int successes = 0;
  for (int i = 0; i < possible; i++) bs[i] = copyBoard((const Board*)b);

  for (int i = 0; successes < possible && i < 8; i++) {
    Board* newB = bs[successes];
    if (makeMove(newB, b->x + dx[i], b->y + dy[i])) {
      bs[successes]->depth++;
      if (possible > 1) {
        pthread_create(&tids[successes], NULL, maxSquares, (void *) newB);
#ifdef NO_PARALLEL
        unsigned int * newMoves;
        pthread_join(tids[successes], (void**)&newMoves);
        printf("THREAD %d: Thread [%d] joined (returned %d)\n", 
               (int)pthread_self(), (int)tids[successes], *(newMoves));
        fflush(stdout);
        *maxMoves = *(newMoves) > *(maxMoves) ? *(newMoves) : *(maxMoves); 
        free(newMoves);
#endif
      }
      else if (possible == 1) {
        unsigned int * newMoves = maxSquares(newB);
        *maxMoves = *(newMoves) > *(maxMoves) ? *(newMoves) : *(maxMoves);
        free(newMoves);
      }
      successes++;
    }
  }
#ifndef NO_PARALLEL
  for (int i = 0; possible > 1 && i < possible; i++) {
    unsigned int * newMoves;
    pthread_join(tids[i], (void**)&newMoves);
    if (newMoves) {
      printf("THREAD %d: Thread %d joined (returned %d)\n", 
             (int)pthread_self(), (int)tids[i], *(newMoves));
      fflush(stdout);
      *maxMoves = *(newMoves) > *(maxMoves) ? *(newMoves) : *(maxMoves); 
      free(newMoves);
    }
  }
#endif
//  for (int i = 0; i < possible; i++) 
  if (b->depth > 0)
    pthread_exit(maxMoves);
  return maxMoves;
}

void knightsTour(Board * b, const int x) {
  b->data[0][0] = ACTIVE;
  b->moves = 1;
  printf("THREAD %d: Solving Sonny's knight's tour problem for a %dx%d board\n", 
         (int)pthread_self(), b->rows, b->cols);
  unsigned int * maxMoves = maxSquares(b);
  free(maxMoves);
  printf("THREAD %d: Best solution(s) found visit %d squares (out of %d)\n", 
         (int)pthread_self(), max_squares, b->cols*b->rows);
  
  printf("THREAD %d: Dead end boards:\n", (int)pthread_self());
#ifdef DEBUG
  printf("%d dead end boards for %d array allocation\n", 
                      dead_end_boards_count, dead_end_boards_size);
#endif
  for (int i = 0; i < dead_end_boards_count; i++) {
    Board * deadB = dead_end_boards[i];
    if (deadB->moves >= x) {
      printBoard(deadB, pthread_self()); 
    }
#ifdef DEBUG
    printBoardDebug(deadB);
#endif
  }
  free(dead_end_boards);
  dead_end_boards_size = 0;
  dead_end_boards_count = 0;
}

void argumentError() {
  fprintf(stderr, "ERROR: Invalid argument(s)\n");
  fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
  exit(1);
}

int main(int argc, const char * argv[]) {
  setvbuf( stdout, NULL, _IONBF, 0 );
  dead_end_boards = calloc(16, sizeof(Board*));
  
  if (argc < 3) { 
    argumentError();
  }
#ifdef DEBUG
    printf("\t---------------------------------------------\n");
    printf("\t--------------------DEBUG--------------------\n");
    printf("\t---------------------------------------------\n");
#endif
  const int m = atoi(argv[1]);
  const int n = atoi(argv[2]);
  int x = INT_MIN;
  if (argc > 3) 
    x = atoi(argv[3]);
  
  if (m <= 2 || n <= 2 || (x != INT_MIN && m*n < x)) {
    argumentError();
  }

  Board * b = makeBoard(m, n);
  knightsTour(b, x);
  
  
  return 0;
}
