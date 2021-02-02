#ifndef GW_VEC2D_C
#  define GW_VEC2D_C

#include <gtk/gtk.h>

typedef struct _vec2d
{
  float x,y;
} vec2d;

void vec2d_rotate_ccw (const vec2d /*in*/ * v, float angle, vec2d /*out*/ * w);
void vec2d_sub (vec2d * v, const vec2d * w);
float vec2d_len (const vec2d * v);
gboolean rect_contains (int left, int top, int right, int bottom,
                        int x, int y);

#endif /* Vec2d.c */
