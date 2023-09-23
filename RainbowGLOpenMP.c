/*Programacao Concorrente e Distribuida Projeto 1
Carlos Guilherme Moraes
Matheus Presotto Limonta
Maycon Andre Mateus Santana
*/
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <SDL2/SDL.h>

#define CELL_SIZE 16

int ROWS, COLS, GENS, NUM_THREADS;

void init(float ***grid, float ***newgrid) {

  int i, j;

  *grid = (float **)malloc(ROWS * sizeof(float *));
  *newgrid = (float **)malloc(ROWS * sizeof(float *));

  #pragma omp parallel private(i, j)
  {
    #pragma omp for
    for (i = 0; i < ROWS; i++) {
      (*grid)[i] = (float *)malloc(COLS * sizeof(float));
      (*newgrid)[i] = (float *)malloc(COLS * sizeof(float));
    }

    #pragma omp for collapse(2)
    for (i = 0; i < ROWS; i++) {
      for (j = 0; j < COLS; j++) {
        (*grid)[i][j] = 0.0;
        (*newgrid)[i][j] = 0.0;
      }
    }
  }

  // GLIDER
  int lin = 1, col = 1;
  (*grid)[lin][col + 1] = 1.0;
  (*grid)[lin + 1][col + 2] = 1.0;
  (*grid)[lin + 2][col] = 1.0;
  (*grid)[lin + 2][col + 1] = 1.0;
  (*grid)[lin + 2][col + 2] = 1.0;

  // R-pentomino
  lin = 10;
  col = 30;
  (*grid)[lin][col + 1] = 1.0;
  (*grid)[lin][col + 2] = 1.0;
  (*grid)[lin + 1][col] = 1.0;
  (*grid)[lin + 1][col + 1] = 1.0;
  (*grid)[lin + 2][col + 1] = 1.0;
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

int getNeighbors(float **grid, int i, int j) {

  int x, y, row, col, alives = 0;
  
  for (x = i - 1; x <= i + 1; x++) {
    for (y = j - 1; y <= j + 1; y++) {

      if (x == i && y == j)
        continue;

      row = (x < 0) ? ROWS - 1 : (x == ROWS) ? 0 : x;
      col = (y < 0) ? COLS - 1 : (y == COLS) ? 0 : y;

      if (grid[row][col] == 1.0)
        alives++;
    }
  }

  return alives;
}

void swap(float ***grid, float ***newgrid) {
  float **temp = *grid;
  *grid = *newgrid;
  *newgrid = temp;
}

void nextGen(float ***grid, float ***newgrid) {
  int i, j, alives;
  float state;

  #pragma omp parallel private(i, j, alives, state) shared(grid)
  {
    #pragma omp for collapse(2)
    for (i = 0; i < ROWS; i++) {
      for (j = 0; j < COLS; j++) {

        alives = getNeighbors((*grid), i, j);
        state = (alives / 8.0) > 0 ? 1.0 : 0.0;

        if ((*grid)[i][j] == 1.0 && (alives == 2 || alives == 3))
          (*newgrid)[i][j] = state;
        else if ((*grid)[i][j] == 0.0 && alives == 3)
          (*newgrid)[i][j] = state;
        else
          (*newgrid)[i][j] = 0.0;
      }
    } 
  }
  swap(grid, newgrid);
}

int drawBoard(SDL_Renderer* renderer, SDL_Event event, float **grid, int i, int j){
  while (SDL_PollEvent(&event)){
    if (event.type == SDL_QUIT) 
      return 1;
  }


  if(i < 5 || i == GENS-1) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < 50; i++) {
      for (int j = 0; j < 50; j++) {
        if (grid[i][j] == 1.0) {
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

  return 0;
}

void closeWindow(SDL_Window* window, SDL_Renderer* renderer, SDL_Event event, int quit){
  // while (!quit) {
  //   while (SDL_PollEvent(&event)) {
  //     if (event.type == SDL_QUIT) {
  //       quit = 1;
  //       break;
  //     }
  //   }
  // }
  SDL_Delay(1500);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int aliveCells(float **grid) {
  int i, j, sum = 0;
  #pragma omp parallel private(i, j) shared(grid) reduction(+ : sum)
  {
    #pragma omp for collapse(2)
    for (i = 0; i < ROWS; i++) {
      for (j = 0; j < COLS; j++) {
        sum += (int)grid[i][j];
      }
    }
  }
  return sum;
}

void freeGrids(float ***grid, float ***newgrid) {
  int i;
  
  for (i = 0; i < ROWS; i++) {
    free((*grid)[i]);
    free((*newgrid)[i]);
  }

  free(*grid);
  free(*newgrid);

  *grid = NULL;
  *newgrid = NULL;
}

int main(int argc, char *argv[]) {
  ROWS = atoi(argv[1]);
  COLS = atoi(argv[1]);
  GENS = atoi(argv[2]);
  NUM_THREADS = atoi(argv[3]);

  int i, j, quit = 0;
  float **grid, **newgrid;

  struct timeval start, finish;
  long long tmili;

  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Event event;

  omp_set_num_threads(NUM_THREADS);
  init(&grid, &newgrid);
  initGUI(&window, &renderer);

  gettimeofday(&start, NULL);
  
  for (int i = 0; i < GENS && !quit; i++) {
    nextGen(&grid, &newgrid);
    quit = drawBoard(renderer, event, grid, i, j);
  }
  gettimeofday(&finish, NULL);
  tmili = (int)(1000 * (finish.tv_sec - start.tv_sec) +
                (finish.tv_usec - start.tv_usec) / 1000);


  printf("Células vivas: %d\n", aliveCells(grid));
  printf("Tempo decorrido: %lldm%.3lfs", (tmili/1000)/60, ((tmili/1000.0)-(tmili/1000))*60);

  freeGrids(&grid, &newgrid);
  closeWindow(window, renderer, event, quit);
}
