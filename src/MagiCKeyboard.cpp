/*
 * Copyright (C) 1990-2025 Andreas Kromke, andreas.kromke@gmail.com
 *
 * This program is free software; you can redistribute it or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
*
* Manages all about the Atari keyboard
*
*/

#include "config.h"
#include <SDL2/SDL_scancode.h>
#include "Debug.h"
#include "Globals.h"
#include "MagiCKeyboard.h"
#include "Atari.h"


/** **********************************************************************************************
 *
 * @brief initialise, currently dummy
 *
 ************************************************************************************************/
int CMagiCKeyboard::init(void)
{
    return 0;
}


/** **********************************************************************************************
 *
 * @brief Convert SDL scancode to Atari scancode
 *
 * @param[in]  s   SDL scancode
 *
 * @return Atari keyboard scancode
 * @retval 0  key not assigned, ignore
 *
 ************************************************************************************************/
uint8_t CMagiCKeyboard::SdlScanCode2AtariScanCode(int s)
{
    switch(s)
    {
        case SDL_SCANCODE_A:              return ATARI_KBD_SCANCODE_A;
        case SDL_SCANCODE_B:              return ATARI_KBD_SCANCODE_B;
        case SDL_SCANCODE_C:              return ATARI_KBD_SCANCODE_C;
        case SDL_SCANCODE_D:              return ATARI_KBD_SCANCODE_D;
        case SDL_SCANCODE_E:              return ATARI_KBD_SCANCODE_E;
        case SDL_SCANCODE_F:              return ATARI_KBD_SCANCODE_F;
        case SDL_SCANCODE_G:              return ATARI_KBD_SCANCODE_G;
        case SDL_SCANCODE_H:              return ATARI_KBD_SCANCODE_H;
        case SDL_SCANCODE_I:              return ATARI_KBD_SCANCODE_I;
        case SDL_SCANCODE_J:              return ATARI_KBD_SCANCODE_J;
        case SDL_SCANCODE_K:              return ATARI_KBD_SCANCODE_K;
        case SDL_SCANCODE_L:              return ATARI_KBD_SCANCODE_L;
        case SDL_SCANCODE_M:              return ATARI_KBD_SCANCODE_M;
        case SDL_SCANCODE_N:              return ATARI_KBD_SCANCODE_N;
        case SDL_SCANCODE_O:              return ATARI_KBD_SCANCODE_O;
        case SDL_SCANCODE_P:              return ATARI_KBD_SCANCODE_P;
        case SDL_SCANCODE_Q:              return ATARI_KBD_SCANCODE_Q;
        case SDL_SCANCODE_R:              return ATARI_KBD_SCANCODE_R;
        case SDL_SCANCODE_S:              return ATARI_KBD_SCANCODE_S;
        case SDL_SCANCODE_T:              return ATARI_KBD_SCANCODE_T;
        case SDL_SCANCODE_U:              return ATARI_KBD_SCANCODE_U;
        case SDL_SCANCODE_V:              return ATARI_KBD_SCANCODE_V;
        case SDL_SCANCODE_W:              return ATARI_KBD_SCANCODE_W;
        case SDL_SCANCODE_X:              return ATARI_KBD_SCANCODE_X;
        case SDL_SCANCODE_Y:              return ATARI_KBD_SCANCODE_Y;
        case SDL_SCANCODE_Z:              return ATARI_KBD_SCANCODE_Z;

        case SDL_SCANCODE_1:              return ATARI_KBD_SCANCODE_1;
        case SDL_SCANCODE_2:              return ATARI_KBD_SCANCODE_2;
        case SDL_SCANCODE_3:              return ATARI_KBD_SCANCODE_3;
        case SDL_SCANCODE_4:              return ATARI_KBD_SCANCODE_4;
        case SDL_SCANCODE_5:              return ATARI_KBD_SCANCODE_5;
        case SDL_SCANCODE_6:              return ATARI_KBD_SCANCODE_6;
        case SDL_SCANCODE_7:              return ATARI_KBD_SCANCODE_7;
        case SDL_SCANCODE_8:              return ATARI_KBD_SCANCODE_8;
        case SDL_SCANCODE_9:              return ATARI_KBD_SCANCODE_9;
        case SDL_SCANCODE_0:              return ATARI_KBD_SCANCODE_0;

        case SDL_SCANCODE_RETURN:         return ATARI_KBD_SCANCODE_RETURN;
        case SDL_SCANCODE_ESCAPE:         return ATARI_KBD_SCANCODE_ESCAPE;
        case SDL_SCANCODE_BACKSPACE:      return ATARI_KBD_SCANCODE_BACKSPACE;
        case SDL_SCANCODE_TAB:            return ATARI_KBD_SCANCODE_TAB;
        case SDL_SCANCODE_SPACE:          return ATARI_KBD_SCANCODE_SPACE;

        case SDL_SCANCODE_MINUS:          return ATARI_KBD_SCANCODE_MINUS;
        case SDL_SCANCODE_EQUALS:         return ATARI_KBD_SCANCODE_EQUALS;
        case SDL_SCANCODE_LEFTBRACKET:    return ATARI_KBD_SCANCODE_LEFTBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET:   return ATARI_KBD_SCANCODE_RIGHTBRACKET;
        case SDL_SCANCODE_NONUSHASH:
        case SDL_SCANCODE_BACKSLASH:      return ATARI_KBD_SCANCODE_BACKSLASH;

        case SDL_SCANCODE_SEMICOLON:      return ATARI_KBD_SCANCODE_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE:     return ATARI_KBD_SCANCODE_APOSTROPHE;
        case SDL_SCANCODE_GRAVE:          return ATARI_KBD_SCANCODE_NUMBER;    // holds '^' and '°' on a German keyboard

        case SDL_SCANCODE_COMMA:          return ATARI_KBD_SCANCODE_COMMA;
        case SDL_SCANCODE_PERIOD:         return ATARI_KBD_SCANCODE_PERIOD;
        case SDL_SCANCODE_SLASH:          return ATARI_KBD_SCANCODE_SLASH;

        case SDL_SCANCODE_CAPSLOCK:       return ATARI_KBD_SCANCODE_CAPSLOCK;

        case SDL_SCANCODE_F1:             return ATARI_KBD_SCANCODE_F1;
        case SDL_SCANCODE_F2:             return ATARI_KBD_SCANCODE_F2;
        case SDL_SCANCODE_F3:             return ATARI_KBD_SCANCODE_F3;
        case SDL_SCANCODE_F4:             return ATARI_KBD_SCANCODE_F4;
        case SDL_SCANCODE_F5:             return ATARI_KBD_SCANCODE_F5;
        case SDL_SCANCODE_F6:             return ATARI_KBD_SCANCODE_F6;
        case SDL_SCANCODE_F7:             return ATARI_KBD_SCANCODE_F7;
        case SDL_SCANCODE_F8:             return ATARI_KBD_SCANCODE_F8;
        case SDL_SCANCODE_F9:             return ATARI_KBD_SCANCODE_F9;
        case SDL_SCANCODE_F10:            return ATARI_KBD_SCANCODE_F10;
        case SDL_SCANCODE_F11:            return ATARI_KBD_SCANCODE_HELP;
        case SDL_SCANCODE_F12:            return ATARI_KBD_SCANCODE_UNDO;

        case SDL_SCANCODE_PRINTSCREEN:    return 0;
        case SDL_SCANCODE_SCROLLLOCK:     return 0;
        case SDL_SCANCODE_PAUSE:          return 0;
        case SDL_SCANCODE_INSERT:         return ATARI_KBD_SCANCODE_INSERT;

        case SDL_SCANCODE_HOME:           return ATARI_KBD_SCANCODE_CLRHOME;
        case SDL_SCANCODE_PAGEUP:         return ATARI_KBD_SCANCODE_PAGEUP;
        case SDL_SCANCODE_DELETE:         return ATARI_KBD_SCANCODE_DELETE;
        case SDL_SCANCODE_END:            return ATARI_KBD_SCANCODE_END;
        case SDL_SCANCODE_PAGEDOWN:       return ATARI_KBD_SCANCODE_PAGEDOWN;
        case SDL_SCANCODE_RIGHT:          return ATARI_KBD_SCANCODE_RIGHT;
        case SDL_SCANCODE_LEFT:           return ATARI_KBD_SCANCODE_LEFT;
        case SDL_SCANCODE_DOWN:           return ATARI_KBD_SCANCODE_DOWN;
        case SDL_SCANCODE_UP:             return ATARI_KBD_SCANCODE_UP;

        case SDL_SCANCODE_NUMLOCKCLEAR:   return 0;

        case SDL_SCANCODE_KP_DIVIDE:      return ATARI_KBD_SCANCODE_KP_DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY:    return ATARI_KBD_SCANCODE_KP_MULTIPLY;
        case SDL_SCANCODE_KP_MINUS:       return ATARI_KBD_SCANCODE_KP_MINUS;
        case SDL_SCANCODE_KP_PLUS:        return ATARI_KBD_SCANCODE_KP_PLUS;
        case SDL_SCANCODE_KP_ENTER:       return ATARI_KBD_SCANCODE_KP_ENTER;
        case SDL_SCANCODE_KP_1:           return ATARI_KBD_SCANCODE_KP_1;
        case SDL_SCANCODE_KP_2:           return ATARI_KBD_SCANCODE_KP_2;
        case SDL_SCANCODE_KP_3:           return ATARI_KBD_SCANCODE_KP_3;
        case SDL_SCANCODE_KP_4:           return ATARI_KBD_SCANCODE_KP_4;
        case SDL_SCANCODE_KP_5:           return ATARI_KBD_SCANCODE_KP_5;
        case SDL_SCANCODE_KP_6:           return ATARI_KBD_SCANCODE_KP_6;
        case SDL_SCANCODE_KP_7:           return ATARI_KBD_SCANCODE_KP_7;
        case SDL_SCANCODE_KP_8:           return ATARI_KBD_SCANCODE_KP_8;
        case SDL_SCANCODE_KP_9:           return ATARI_KBD_SCANCODE_KP_9;
        case SDL_SCANCODE_KP_0:           return ATARI_KBD_SCANCODE_KP_0;
        case SDL_SCANCODE_KP_PERIOD:      return ATARI_KBD_SCANCODE_KP_PERIOD;

        case SDL_SCANCODE_NONUSBACKSLASH: return ATARI_KBD_SCANCODE_LTGT;    // <> on German keyboard

        case SDL_SCANCODE_APPLICATION:    return 0;
        case SDL_SCANCODE_POWER:          return 0;

        case SDL_SCANCODE_KP_EQUALS:      return 0;

        case SDL_SCANCODE_LCTRL:          return ATARI_KBD_SCANCODE_CONTROL;
        case SDL_SCANCODE_LSHIFT:         return ATARI_KBD_SCANCODE_LSHIFT;
        case SDL_SCANCODE_LALT:           return ATARI_KBD_SCANCODE_ALT;
        case SDL_SCANCODE_LGUI:           return 0;
        case SDL_SCANCODE_RCTRL:          return ATARI_KBD_SCANCODE_CONTROL;
        case SDL_SCANCODE_RSHIFT:         return ATARI_KBD_SCANCODE_RSHIFT;
        case SDL_SCANCODE_RALT:           return ATARI_KBD_SCANCODE_ALTGR;
        case SDL_SCANCODE_RGUI:           return 0;

        default: return 0;
    }
}
