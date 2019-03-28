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
 */



struct Node {
  char * value;
  int index;
};
typedef struct Node Node;

int hash(const char * str) {
  int h = 0;
  for (const char * p = str; *p != '\0'; p++) {
    h += *p;
  }
  return h;
}

struct HashTable {
  // {a, b, c, d, e, f, g, ...}
  // {Occurrence(a, 1), Occurrence(b, 2), ...}
  Node ** values;
  unsigned long size, allocated;
};
typedef struct HashTable nGram;

nGram* makeNGram(unsigned long size) {
  nGram *new = (nGram*)calloc(1, sizeof(nGram));
  new->values = (Node**)calloc(size, sizeof(Node*));
  new->size = size;
  new->allocated = 0;
  return new;
}

Node* getByIndex(const nGram* n, const int index) {
  return index < n->size ? *(n->values + index) : NULL;
}
Node* get(const nGram* n, const char* key) {
  int index = hash(key)%(n->size);
  return getByIndex(n, index);
}

Node* put(nGram *n, const char * str) {
  int h = hash(str);
  Node * o = getByIndex(n, h);
  if (!o) {
    o = (Node*)calloc(1, sizeof(Node));
    o->index = h%n->size;
    o->value = (char*)calloc(strlen(str)+1, sizeof(char));
    strcpy(o->value, str);
    *((n->values) + (h%n->size)) = o;
    n->allocated++;
  } else {
    o->value = realloc(o->value, (strlen(str)+1)*sizeof(char));
    strcpy(o->value, str);
  }
  
  return o;
}



struct Board {
  int ** data;
  int rows;
  int cols;
  int x;
  int y;
  int moves;
  int depth;
}; 
typedef struct Board Board;

int isDebug = 0==1;

int max_squares = 0;
Board ** dead_end_boards = NULL;

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
  board->data = calloc(rows, sizeof(int*));
  for (int i = 0; i < rows; i++) {
    board->data[i] = calloc(cols, sizeof(int));
    for (int j = 0; j < cols; j++)
      board->data[i][j] = EMPTY;
  }
  return board;
}
Board * copyBoard(const Board * b) {
  Board* newB = makeBoard(b->rows, b->cols);
  newB->x = b->x;
  newB->y = b->y;
  newB->depth = b->depth;
  newB->moves = b->moves;
  for (int i = 0; i < b->rows; i++)
    for (int j = 0; j < b->cols; j++)
      newB->data[i][j] = b->data[i][j];
  return newB;
}


int itemAt(const Board * b, const int row, const int col) {
  if (row < 0 || col < 0 || row >= b->rows || col >= b->cols) 
    return INVALID;
  else return b->data[row][col]; 
}

int itemIs(Board * b, const int row, const int col, int new) {
  int old = itemAt(b, row, col);
  if (old != INVALID) 
    b->data[row][col] = new;
   return old;
}

void printLine(int n) {
  for (int i = 0; i < n; i++) {
    printf("+---"); 
  }
  printf("+\n");
}

void printBoard(const Board* b) {
  for (int i = 0; i < b->rows; i++){
    printLine(b->cols);
    for (int j = 0; j < b->cols; j++) {
      int item = itemAt(b, i, j);
      if (item == INVALID) 
        printf("| ! ");
      else if (item == EMPTY)
        printf("|   ");
      else if (item == ACTIVE)
        printf("|<S>");
      else
        printf("| %d ", item);
    }
    printf("|\n");
  }
  printLine(b->cols);
}

int isFree(const Board * b, const int r, const int c) {
  return itemAt(b, r, c) == EMPTY;
}

int makeMove(Board * b, int xp, int yp) {
  if (!isFree(b, xp, yp)) 
    return 0==1;
  itemIs(b, b->x, b->y, b->moves++);
  itemIs(b, xp, yp, ACTIVE); 
  b->x = xp;
  b->y = yp;
  return 0==0;
}
void undoMove(Board *b, int xo, int yo) {
  itemIs(b, b->x, b->y, EMPTY);
  itemIs(b, xo, yo, --b->moves);
  b->x = xo;
  b->y = yo;
}
int possibleMoveCount(const Board *b) {
  const int ds[8] = {1, 2,  2,  1, -1, -2, -2, -1};
  int moveCount = 0;
  for (int i = 0; i < 8; i++) 
    moveCount += isFree(b, b->x + ds[(i+2)%8], b->y + ds[i]);
  return moveCount;
}


void * maxSquares(void * argv) {
  Board * b = (Board*) argv;
  const int ds[8] = {1, 2,  2,  1, -1, -2, -2, -1};
  
  if (b->moves == b->rows * b->cols) {
    return NULL; 
  } else if (b->moves > b->rows * b->cols) {
    perror("Impossible number of moves made");
    return NULL; 
  }
  
  // Call threads for up, down, right, left
  int possible = possibleMoveCount(b);
  if (possible > 1) 
    printf("THREAD %d: %d moves possible after move #%d; creating threads...\n", (int)pthread_self(), possible, b->moves);
  else if (possible == 0) {
    printf("THREAD %d: Dead end after move #%d\n", (int)pthread_self(), b->moves);
    max_squares = max_squares < b->moves ? b->moves : max_squares;
    return NULL;
  }
  pthread_t tids[possible];
  Board ** bs = calloc(possible, sizeof(Board));
  int successes = 0;
  for (int i = 0; i < 8; i++) {
    Board* newB = copyBoard((const Board*)b); 
    bs[successes] = newB;
    if (makeMove(newB, b->x + ds[(i+2)%8], b->y + ds[i])) {
      if (possible > 1) {
        pthread_create(&tids[successes], NULL, maxSquares, (void *) newB);
#ifdef NO_PARALLEL
        pthread_join(tids[successes], NULL );
        printf("THREAD %d: Thread %d joined (returned %d)\n", (int)pthread_self(), (int)tids[successes], bs[i]->moves);
#endif
      }
      else if (possible == 1)
        maxSquares(newB);
      successes++;
    } 
  }
#ifndef NO_PARALLEL
  for (int i = 0; b->depth == 0 && i < possible && possible > 1; i++) {
    pthread_join(tids[i], NULL );
    printf("THREAD %d: Thread %d joined (returned %d)\n", (int)pthread_self(), (int)tids[i], bs[i]->moves);
    freeBoard(bs[i]);
  }
#endif
  return NULL;
}

void knightsTour(Board * b, const int x) {
  b->data[0][0] = 0;
  b->moves = 1;
  printf("THREAD %d: Solving Sonny's knight's tour problem for a %dx%d board\n", (int)pthread_self(), b->cols, b->rows);
  maxSquares(b);
  printf("THREAD %d: Best solution(s) found visit %d squares (out of %d)\n", (int)pthread_self(), max_squares, b->cols*b->rows);
  
  printf("THREAD %d: Dead end boards:\n", (int)pthread_self());
  /*
   THREAD 1000: > SSS
   THREAD 1000: S.S
   THREAD 1000: SSS
   THREAD 1000: > SSS
   THREAD 1000: S.S
   THREAD 1000: SSS
   */
  /*
   If all squares are visited 
   print the solution
   Else
   a) Add one of the next moves to solution vector and recursively 
   check if this move leads to a solution. (A Knight can make maximum 
   eight moves. We choose one of the 8 moves in this step).
   b) If the move chosen in the above step doesn't lead to a solution
   then remove this move from the solution vector and try other 
   alternative moves.
   c) If none of the alternatives work then return false (Returning false 
   will remove the previously added item in recursion and if false is 
   returned by the initial call of recursion then "no solution exists" )   

   */
  
  
}



int main(int argc, const char * argv[]) {
  setvbuf( stdout, NULL, _IONBF, 0 );
  if (argc < 3) 
    perror("Invalid number of arguments (2 required)"); 
  isDebug = strcmp(argv[argc-1], "debug") == 0;
  const int m = atoi(argv[1]);
  const int n = atoi(argv[2]);
  int x = -1;
  if (argc > 3) 
    x = atoi(argv[3]);
  
  if (m <= 2 || n <= 2) {
    fprintf(stderr, "ERROR: Invalid argument(s)\n");
    fprintf(stderr, "USAGE: a.out <m> <n> [<x>]\n");
  }

  Board * b = makeBoard(m, n);
  knightsTour(b, x);
  
  return 0;
}
