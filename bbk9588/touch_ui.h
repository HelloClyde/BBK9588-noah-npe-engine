#ifndef NPE_TOUCH_UI_H
#define NPE_TOUCH_UI_H

#include <SDL.h>

enum { NPE_UI_PAD, NPE_UI_ALPHA, NPE_UI_SYMBOLS, NPE_UI_FUNCTIONS };
enum { NPE_TOUCH_PAD=-1, NPE_TOUCH_QUIT=-2, NPE_TOUCH_ALPHA=-3,
       NPE_TOUCH_SYMBOLS=-4, NPE_TOUCH_FUNCTIONS=-5, NPE_TOUCH_SHIFT=-6,
       NPE_TOUCH_CAPS=-7, NPE_TOUCH_CTRL=-8, NPE_TOUCH_ALT=-9,
       NPE_TOUCH_VOL_DOWN=-10, NPE_TOUCH_VOL_UP=-11, NPE_TOUCH_MUTE=-12,
       NPE_TOUCH_ABOUT=-13 };
#define NPE_HEADER_X 7
#define NPE_HEADER_Y 1
/* Drawing and hit testing share geometry on every keyboard page. */
int npe_touch_hit(int page, int x, int y);
void npe_touch_draw(Uint8 *pixels, int page, Uint16 modifiers, int volume,
                    int audio_error, int pressed_x, int pressed_y);

#endif
