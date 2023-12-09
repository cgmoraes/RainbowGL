#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <SDL2/SDL.h>

#define CELL_SIZE 16

int ROWS, COLS, GENS;

void init(float **grid, float **newgrid) {

  int i, j;

  for (i = 0; i < ROWS; i++) {
    for (j = 0; j < COLS; j++) {
      (*grid)[i * COLS + j] = 0.0;
      (*newgrid)[i * COLS + j] = 0.0;
    }
  }

  // GLIDER
  int lin = 1, col = 1;
  (*grid)[lin * COLS + col + 1] = 1.0;
  (*grid)[(lin + 1) * COLS + col + 2] = 1.0;
  (*grid)[(lin + 2) * COLS + col] = 1.0;
  (*grid)[(lin + 2) * COLS + col + 1] = 1.0;
  (*grid)[(lin + 2) * COLS + col + 2] = 1.0;

  // R-pentomino
  lin = 10;
  col = 30;
  (*grid)[lin * COLS + col + 1] = 1.0;
  (*grid)[lin * COLS + col + 2] = 1.0;
  (*grid)[(lin + 1) * COLS + col] = 1.0;
  (*grid)[(lin + 1) * COLS + col + 1] = 1.0;
  (*grid)[(lin + 2) * COLS + col + 1] = 1.0;
}

void initGUI(SDL_Window** window, SDL_Renderer** renderer){
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("Error initializing SDL: %s\n", SDL_GetError());
    exit(1);
  }

  *window = SDL_CreateWindow("Rainbow Game of Life", 0, 0, 800, 800, SDL_WINDOW_SHOWN);
  if (*window == NULL) {
    printf("Error creating window: %s\n", SDL_GetError());
    exit(1);
  }

  *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
  if (*renderer == NULL) {
    printf("Error creating renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(*window);
    SDL_Quit();
    exit(1);
  }
}

int getNeighbors(float *grid, int i, int j) {

  int x, y, row, col, alives = 0;
  
  for (x = i - 1; x <= i + 1; x++) {
    for (y = j - 1; y <= j + 1; y++) {

      if (x == i && y == j)
        continue;

      row = (x < 0) ? ROWS - 1 : (x == ROWS) ? 0 : x;
      col = (y < 0) ? COLS - 1 : (y == COLS) ? 0 : y;

      if (grid[row * COLS + col] == 1.0)
        alives++;
    }
  }

  return alives;
}

void swap(float **grid, float **newgrid, int noProcesses, int processId, int localSize, int noLines) {
  int i;
  float *temp= *grid;
  *grid = *newgrid;
  *newgrid = temp;

  // Envia cada parcela do newgrid obtido por cada processo para compor o grid do processo master (0)
  if(processId==0 && noProcesses > 1){
    for (i = 1; i < noProcesses; i++) {
      if(i == noProcesses-1) MPI_Recv(&(*grid)[i * localSize * COLS], (ROWS*COLS)-(i*localSize*COLS), MPI_FLOAT, i, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      else MPI_Recv(&(*grid)[i * localSize * COLS], localSize * COLS, MPI_FLOAT, i, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
  } else if (noProcesses > 1){
    MPI_Send(&(*grid)[processId * noLines * COLS], noLines * COLS, MPI_FLOAT, 0, 12, MPI_COMM_WORLD);
  }
}

void nextGen(float **grid, float **newgrid, int start, int finish) {
  int i, j, alives;
  float state;

  for (i = start; i < finish; i++) {
    for (j = 0; j < COLS; j++) {

      alives = getNeighbors((*grid), i, j);
      state = (alives / 8.0) > 0 ? 1.0 : 0.0;

      if ((*grid)[i * COLS + j] == 1.0 && (alives == 2 || alives == 3))
        (*newgrid)[i * COLS + j] = state;
      else if ((*grid)[i * COLS + j] == 0.0 && alives == 3)
        (*newgrid)[i * COLS + j] = state;
      else
        (*newgrid)[i * COLS + j] = 0.0;
    }
  } 
}

void drawBoard(SDL_Renderer* renderer, SDL_Event event, float *grid, int gen){
  if(gen < 5 || gen == GENS-1) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    for (int i = 0; i < 50; i++) {
      for (int j = 0; j < 50; j++) {
        if (grid[i * COLS + j] == 1.0) {
          SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        } else {
          SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        }

        SDL_Rect cellRect;
        cellRect.x = j * CELL_SIZE;
        cellRect.y = i * CELL_SIZE;
        cellRect.w = CELL_SIZE;
        cellRect.h = CELL_SIZE;
        SDL_RenderFillRect(renderer, &cellRect);
      }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(500);
  }
}

void closeWindow(SDL_Window* window, SDL_Renderer* renderer, SDL_Event event){
  SDL_Delay(1500);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void aliveCells(float *grid, int *totalSum, int start, int finish) {
  int i, j, sum = 0;

  for (i = start; i < finish; i++) {
    for (j = 0; j < COLS; j++) {
      sum += (int)grid[i * COLS + j];
    }
  }
  MPI_Reduce(&sum, totalSum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
}

void freeGrids(float **grid, float **newgrid) {
  free(*grid);
  free(*newgrid);
}

int main(int argc, char *argv[]) {
  
  ROWS = 2048;
  COLS = ROWS;
  GENS = 2000;

  float *grid, *newgrid;
  int i, j, noProcesses, processId, localSize, first, noLines, sum;

  double start, finish, time;

  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Event event;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &noProcesses);
  MPI_Comm_rank(MPI_COMM_WORLD, &processId);

  localSize = ROWS/noProcesses;
  first = processId*localSize;

  if (processId==noProcesses-1) noLines = ROWS - first;
  else noLines = localSize;

  grid = (float *)malloc(ROWS * COLS * sizeof(float));
  newgrid = (float *)malloc(ROWS * COLS * sizeof(float));

  init(&grid, &newgrid); 

  if(processId==0){
    initGUI(&window, &renderer);
    start = MPI_Wtime();
  }
  
  for (int i = 0; i < GENS; i++) {
    nextGen(&grid, &newgrid, first, first+noLines);

    MPI_Barrier(MPI_COMM_WORLD);
    swap(&grid, &newgrid, noProcesses, processId, localSize, noLines);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(grid, ROWS*COLS, MPI_FLOAT, 0, MPI_COMM_WORLD);

    if(processId == 0) drawBoard(renderer, event, grid, i);
  }

  aliveCells(grid, &sum, first, first+noLines);

  if(processId==0){
    finish = MPI_Wtime();   
    time = finish-start; 
    printf("Células vivas: %d\n", sum);
    printf("Tempo decorrido: %dm%.3lfs\n", (int)time/60, (int)time%60 + (time-(int)time));
    closeWindow(window, renderer, event);
  }
  freeGrids(&grid, &newgrid);
  MPI_Finalize();  
}