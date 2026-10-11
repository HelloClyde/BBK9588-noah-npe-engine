#ifndef NPE_TOUCH_UI_H
#define NPE_TOUCH_UI_H

#include <SDL.h>

/* Hit testing and drawing use the same key geometry. Negative keys select
   the other panel (-1) or exit the application (-2). */
int npe_touch_hit(int keyboard, int x, int y);
void npe_touch_draw(Uint8 *pixels, int keyboard, int pressed_x, int pressed_y);

#endif
