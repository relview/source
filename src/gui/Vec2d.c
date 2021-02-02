#include "Vec2d.h"

#include <assert.h>
#include <math.h>

void vec2d_rotate_ccw (const vec2d /*in*/ * v, float angle, vec2d /*out*/ * w)
{
  assert (w != NULL && v != NULL);

  w->x = cos(angle) * v->x + sin(angle) * v->y;
  w->y = -sin(angle) * v->x + cos(angle) * v->y;
}

void vec2d_sub (vec2d * v, const vec2d * w)
{
  v->x -= w->x;
  v->y -= w->y;
}


float vec2d_len (const vec2d * v)
{
  return sqrt (v->x*v->x + v->y*v->y);
}


gboolean rect_contains (int left, int top, int right, int bottom,
                        int x, int y)
{
  if (left > right)
    return rect_contains (right,top,left,bottom,x,y);
  if (top > bottom)
    return rect_contains (left,bottom,right,top,x,y);

  return top <= y && y <= bottom
    && left <= x && x <= right;
}
