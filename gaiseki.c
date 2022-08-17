#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define REAL_SCREEN_WIDTH 1000
#define REAL_SCREEN_HEIGHT 1000

#define VIRT_SCREEN_WIDTH 500
#define VIRT_SCREEN_HEIGHT 500

#define PX_WIDTH REAL_SCREEN_WIDTH / VIRT_SCREEN_WIDTH
#define PX_HEIGHT REAL_SCREEN_HEIGHT / VIRT_SCREEN_HEIGHT

typedef SDL_Surface S;

typedef enum {
  ADD_POINT,
  MOVE_POINT,
} CURR_TOOL;

typedef struct {
  uint32_t x;
  uint32_t y;
} P;

typedef struct {
  P *p;
  uint8_t np;
} C;

typedef struct {
  SDL_Window *window;
  S *s;
  void *hover;
  CURR_TOOL t;
  C c;
} A;

void initApp(A *app) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("Couldn't initialize SDL: %s\n", SDL_GetError());
    exit(1);
  }

  app->window =
      SDL_CreateWindow("外積", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       REAL_SCREEN_WIDTH, REAL_SCREEN_HEIGHT, 0);

  if (!app->window) {
    printf("Failed to open window: %s\n", SDL_GetError());
    exit(1);
  }

  SDL_ShowCursor(SDL_DISABLE);
  app->c.np = 0;
  app->c.p = NULL;
  app->hover = NULL;
  app->t = ADD_POINT;
  app->s = SDL_GetWindowSurface(app->window);
}

// paint pixel
void pxp(SDL_Surface *s, uint64_t x, uint64_t y, uint8_t r, uint8_t g,
         uint8_t b) {
  for (int dy = 0; dy < PX_HEIGHT; ++dy) {
    for (int dx = 0; dx < PX_WIDTH; ++dx) {
      *((int *)s->pixels + (x * PX_WIDTH + dx) + (y * PX_HEIGHT + dy) * s->w) =
          SDL_MapRGB(s->format, r, g, b);
    }
  }
}

// draw background
void d_bg(S *s) {
  memset((uint32_t *)s->pixels, 0, s->w * sizeof(uint32_t) * s->h);
  for (int x = 15; x < VIRT_SCREEN_WIDTH; x += 20) {
    for (int y = 15; y < VIRT_SCREEN_HEIGHT; y += 20) {
      pxp(s, x, y, 80, 80, 80);
    }
  }
}

// draw cursor
void d_cur(S *s, uint64_t cx, uint64_t cy) {
  for (int x = -5; x <= 5; x++) {
    int32_t tpx = cx + x;
    if (tpx >= 0 && tpx < VIRT_SCREEN_WIDTH) {
      pxp(s, cx + x, cy, 200, 200, 200);
    }
  }

  for (int y = -5; y <= 5; y++) {
    int32_t tpy = cy + y;
    if (tpy >= 0 && tpy < VIRT_SCREEN_HEIGHT) {
      pxp(s, cx, cy + y, 200, 200, 200);
    }
  }
}

// linear interpolation
P lerp(P *p1, P *p2, double t) {
  P r;
  double x = (double)p1->x + t * ((double)p2->x - (double)p1->x);
  double y = (double)p1->y + t * ((double)p2->y - (double)p1->y);
  r.x = (int)floorf(x);
  r.y = (int)floorf(y);
  return r;
}

// inner recursive lerp
P _rlerp(P *r, P *p, int np, double t) {
  for (int i = 0; i < np - 1; i++) {
    r[i] = lerp(&p[i], &p[i + 1], t);
  }

  if (np == 1) {
    return r[0];
  }

  return _rlerp(r, r, np - 1, t);
}

// recursive lerp
P rlerp(P *p, int np, double t) {
  P *r = (P *)malloc(np * sizeof(P));
  P res = _rlerp(r, p, np, t);
  free(r);
  return res;
}

void d_dl(S *s, P *p1, P *p2, uint8_t r, uint8_t g, uint8_t b) {
  for (float t = 0; t < 1.0f; t += 0.01f) {
    P p = lerp(p1, p2, t);
    pxp(s, p.x, p.y, r, g, b);
  }
}

// draw curve
void d_c(A *a, C *c) {
  S *s = a->s;
  for (int i = 0; i < c->np; ++i) {
    P *p = &c->p[i];
    for (int dx = -2; dx <= 2; dx++) {
      for (int dy = -2; dy <= 2; dy++) {
        uint8_t h = 128;
        if (a->hover == p) {
          h = 255;
        }
        pxp(s, p->x + dx, p->y + dy, h, h, h);
      }
    }
  }

  if (c->np < 2)
    return;

  for (float t = 0; t <= 1.0f; t += 0.001f) {
    P p = rlerp(c->p, c->np, t);
    pxp(s, p.x, p.y, 255, 0, 0);
  }

  if (c->np == 3) {
    for (int i = 0; i < c->np - 1; i++) {
      d_dl(s, &c->p[i], &c->p[i + 1], 100, 100, 100);
    }
  } else if (c->np > 3) {
    for (int i = 0; i < c->np - 1; i += 2) {
      d_dl(s, &c->p[i], &c->p[i + 1], 100, 100, 100);
    }
  }
}

// add point to curve
P *c_ap(C *c) {
  P *npa = (P *)malloc(++c->np * sizeof(P));
  if (c->p != NULL) {
    memcpy(npa, c->p, (c->np - 1) * sizeof(P));
    free(c->p);
  }
  c->p = npa;
  return &npa[c->np - 1];
}

int main() {
  A app;

  initApp(&app);

  S *surf = SDL_GetWindowSurface(app.window);
  P cur = {.x = 0, .y = 0};

  while (1) {
    SDL_Event event;

    SDL_WaitEvent(&event);

    if (SDL_QUIT == event.type) {
      exit(0);
    } else if (SDL_MOUSEBUTTONDOWN == event.type) {
      int cx = div(event.button.x, PX_WIDTH).quot;
      int cy = div(event.button.y, PX_HEIGHT).quot;
      P *np = c_ap(&app.c);
      np->x = cx;
      np->y = cy;
    } else if (SDL_MOUSEBUTTONUP == event.type) {
      if (app.t == MOVE_POINT) {
        app.t = ADD_POINT;
      }
    } else if (SDL_MOUSEMOTION == event.type) {
      int cx = div(event.button.x, PX_WIDTH).quot;
      int cy = div(event.button.y, PX_HEIGHT).quot;

      for (int i = 0; i < app.c.np; i++) {
        P *ip = &app.c.p[i];
        if (ip->x == cx && ip->y == cy) {
          app.hover = ip;
        }
      }

      cur.x = cx;
      cur.y = cy;
    }

    if (SDL_MUSTLOCK(surf)) {
      SDL_LockSurface(surf);
    }

    d_bg(surf);
    if (app.t != MOVE_POINT) {
      d_cur(surf, cur.x, cur.y);
    }
    d_c(&app, &app.c);

    if (SDL_MUSTLOCK(surf)) {
      SDL_UnlockSurface(surf);
    }

    SDL_UpdateWindowSurface(app.window);
  }
  SDL_DestroyWindow(app.window);

  SDL_Quit();
  return 0;
}
