#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define REAL_SCREEN_WIDTH 1000
#define REAL_SCREEN_HEIGHT 1000

#define VIRT_SCREEN_WIDTH 500
#define VIRT_SCREEN_HEIGHT 500

#define PX_WIDTH (int32_t)(REAL_SCREEN_WIDTH / VIRT_SCREEN_WIDTH)
#define PX_HEIGHT (int32_t)(REAL_SCREEN_HEIGHT / VIRT_SCREEN_HEIGHT)

typedef SDL_Surface S;

typedef enum {
  ADD_POINT,
  MOVE_POINT,
} CURR_TOOL;

typedef struct {
  int32_t x;
  int32_t y;
  float sz;
} P;

typedef struct {
  float x;
  float y;
} Pf;

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
void pxp(S *s, uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b,
         float sz) {
  for (int32_t dy = 0; dy < PX_HEIGHT * sz; ++dy) {
    for (int32_t dx = 0; dx < PX_WIDTH * sz; ++dx) {
      *((int *)s->pixels + (x * PX_WIDTH + dx) + (y * PX_HEIGHT + dy) * s->w) =
          SDL_MapRGB(s->format, r, g, b);
    }
  }
}

// paint pixel
void upxp(S *s, uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b) {
  pxp(s, x, y, r, g, b, 1);
}

void d_l(S *s, P *p1, P *p2, uint8_t r, uint8_t g, uint8_t b, uint8_t sz) {
  int32_t dx = p2->x - p1->x;
  int32_t dy = p2->y - p1->y;

  float len = sqrt(dx * dx + dy * dy);
  Pf dvl = {.x = (float)dx / len, .y = (float)dy / len};
  Pf cp = {.x = (float)p1->x, .y = (float)p1->y};

  while (fabs(p2->x - cp.x) > 0.5f && fabs(p2->y - cp.y) > 0.5f) {
    pxp(s, round(cp.x), round(cp.y), r, g, b, sz);
    cp.x += dvl.x;
    cp.y += dvl.y;
  }
}

// draw background
void d_bg(S *s) {
  memset((int32_t *)s->pixels, 0, s->w * sizeof(int32_t) * s->h);
  for (int x = 15; x < VIRT_SCREEN_WIDTH; x += 20) {
    for (int y = 15; y < VIRT_SCREEN_HEIGHT; y += 20) {
      upxp(s, x, y, 80, 80, 80);
    }
  }
}

// draw cursor
void d_cur(S *s, uint64_t cx, uint64_t cy) {
  for (int x = -5; x <= 5; x++) {
    int32_t tpx = cx + x;
    if (tpx >= 0 && tpx < VIRT_SCREEN_WIDTH) {
      upxp(s, cx + x, cy, 200, 200, 200);
    }
  }

  for (int y = -5; y <= 5; y++) {
    int32_t tpy = cy + y;
    if (tpy >= 0 && tpy < VIRT_SCREEN_HEIGHT) {
      upxp(s, cx, cy + y, 200, 200, 200);
    }
  }
}

float flerp(float f1, float f2, double t) { return f1 + t * (f2 - f1); }

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
  for (double t = 0; t < 1.0; t += 0.01) {
    P p = lerp(p1, p2, t);
    upxp(s, p.x, p.y, r, g, b);
  }
}

int32_t clamp(int32_t p, int32_t min, int32_t max) {
  if (p > max) return max;
  if (p < min) return min;
  return p;
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
        upxp(s, p->x + dx, p->y + dy, h, h, h);
      }
    }
  }

  if (c->np < 2)
    return;

  P p = rlerp(c->p, c->np, 0);
  for (double t = 0; t <= 1.0; t += 0.001) {
    P np = rlerp(c->p, c->np, t);
    float sz = flerp(c->p[0].sz, c->p[c->np - 1].sz, t);
    d_l(s, &p, &np, 255, 0, 0, sz);
    p = np;
    pxp(s, p.x, p.y, 255, 0, 0, sz);
  }

  if (c->np == 3) {
    for (int i = 0; i < c->np - 1; i++) {
      d_l(s, &c->p[i], &c->p[i + 1], 100, 100, 100, 1);
    }
  } else if (c->np > 3) {
    for (int i = 0; i < c->np - 1; i += 2) {
      d_l(s, &c->p[i], &c->p[i + 1], 100, 100, 100, 1);
    }
  }

  for (int i = 0; i < c->np; ++i) {
    P *p = &c->p[i];
    for (int dx = -2; dx <= 2; dx++) {
      for (int dy = -2; dy <= 2; dy++) {
        uint8_t h = 128;
        if (a->hover == p) {
          h = 255;
        }
        upxp(s, p->x + dx, p->y + dy, h, h, h);
      }
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
      if (app.hover) {
        app.t = MOVE_POINT;
      }

      int cx = div(event.button.x, PX_WIDTH).quot;
      int cy = div(event.button.y, PX_HEIGHT).quot;
      if (app.t == ADD_POINT) {
        P *np = c_ap(&app.c);
        np->x = clamp(cx, 0, VIRT_SCREEN_WIDTH);
        np->y = clamp(cy, 0, VIRT_SCREEN_HEIGHT);
        np->sz = 4;
      }
    } else if (SDL_MOUSEBUTTONUP == event.type) {
      if (app.t == MOVE_POINT) {
        app.t = ADD_POINT;
      }
    } else if (SDL_MOUSEWHEEL == event.type) {
      printf("WASSUP\n");
      if (app.hover) {
        printf("%d\n", event.wheel.y);
        if (event.wheel.y > 0) {
          ((P *)app.hover)->sz += 0.5f;
          printf("?? %f\n", ((P *)app.hover)->sz);
        } else if (event.wheel.y < 0) {
          ((P *)app.hover)->sz -= 0.2f;
        }
      }
    } else if (SDL_MOUSEMOTION == event.type) {
      int cx = div(event.button.x, PX_WIDTH).quot;
      int cy = div(event.button.y, PX_HEIGHT).quot;

      if (app.hover && app.t == MOVE_POINT) {
        ((P *)app.hover)->x = clamp(cx, 0, VIRT_SCREEN_WIDTH);
        ((P *)app.hover)->y = clamp(cy, 0, VIRT_SCREEN_HEIGHT);
      } else {
        app.hover = NULL;
        for (int i = 0; i < app.c.np; i++) {
          P *ip = &app.c.p[i];
          if (ip->x >= (cx - PX_WIDTH) && ip->x <= (cx + PX_WIDTH) &&
              ip->y >= (cy - PX_WIDTH) && ip->y <= (cy + PX_WIDTH)) {
            app.hover = ip;
          }
        }
      }

      cur.x = clamp(cx, 0, VIRT_SCREEN_WIDTH);
      cur.y = clamp(cy, 0, VIRT_SCREEN_HEIGHT);
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
