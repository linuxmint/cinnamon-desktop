/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *	Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __CDesktop_enums_h__
#define __CDesktop_enums_h__

typedef enum
{
  C_DESKTOP_PROXY_MODE_NONE,
  C_DESKTOP_PROXY_MODE_MANUAL,
  C_DESKTOP_PROXY_MODE_AUTO
} CDesktopProxyMode;

typedef enum
{
  C_DESKTOP_TOOLBAR_STYLE_BOTH,
  C_DESKTOP_TOOLBAR_STYLE_BOTH_HORIZ,
  C_DESKTOP_TOOLBAR_STYLE_ICONS,
  C_DESKTOP_TOOLBAR_STYLE_TEXT
} CDesktopToolbarStyle;

typedef enum
{
  C_DESKTOP_TOOLBAR_ICON_SIZE_SMALL,
  C_DESKTOP_TOOLBAR_ICON_SIZE_LARGE
} CDesktopToolbarIconSize;

typedef enum
{
  C_DESKTOP_BACKGROUND_STYLE_NONE,
  C_DESKTOP_BACKGROUND_STYLE_WALLPAPER,
  C_DESKTOP_BACKGROUND_STYLE_CENTERED,
  C_DESKTOP_BACKGROUND_STYLE_SCALED,
  C_DESKTOP_BACKGROUND_STYLE_STRETCHED,
  C_DESKTOP_BACKGROUND_STYLE_ZOOM,
  C_DESKTOP_BACKGROUND_STYLE_SPANNED
} CDesktopBackgroundStyle;

typedef enum
{
  C_DESKTOP_BACKGROUND_SHADING_SOLID,
  C_DESKTOP_BACKGROUND_SHADING_VERTICAL,
  C_DESKTOP_BACKGROUND_SHADING_HORIZONTAL
} CDesktopBackgroundShading;

typedef enum
{
  C_DESKTOP_MOUSE_DWELL_MODE_WINDOW,
  C_DESKTOP_MOUSE_DWELL_MODE_GESTURE
} CDesktopMouseDwellMode;

typedef enum
{
  C_DESKTOP_MOUSE_DWELL_DIRECTION_LEFT,
  C_DESKTOP_MOUSE_DWELL_DIRECTION_RIGHT,
  C_DESKTOP_MOUSE_DWELL_DIRECTION_UP,
  C_DESKTOP_MOUSE_DWELL_DIRECTION_DOWN
} CDesktopMouseDwellDirection;

typedef enum
{
  C_DESKTOP_SCREENSAVER_MODE_BLANK_ONLY,
  C_DESKTOP_SCREENSAVER_MODE_RANDOM,
  C_DESKTOP_SCREENSAVER_MODE_SINGLE
} CDesktopScreensaverMode;

typedef enum
{
  C_DESKTOP_MAGNIFIER_MOUSE_TRACKING_MODE_NONE,
  C_DESKTOP_MAGNIFIER_MOUSE_TRACKING_MODE_CENTERED,
  C_DESKTOP_MAGNIFIER_MOUSE_TRACKING_MODE_PROPORTIONAL,
  C_DESKTOP_MAGNIFIER_MOUSE_TRACKING_MODE_PUSH
} CDesktopMagnifierMouseTrackingMode;

typedef enum
{
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_NONE,
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_FULL_SCREEN,
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_TOP_HALF,
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_BOTTOM_HALF,
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_LEFT_HALF,
  C_DESKTOP_MAGNIFIER_SCREEN_POSITION_RIGHT_HALF,
} CDesktopMagnifierScreenPosition;

typedef enum
{
  C_DESKTOP_TITLEBAR_ACTION_TOGGLE_SHADE = 0,
  C_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE,
  C_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_HORIZONTALLY,
  C_DESKTOP_TITLEBAR_ACTION_TOGGLE_MAXIMIZE_VERTICALLY,
  C_DESKTOP_TITLEBAR_ACTION_MINIMIZE,
  C_DESKTOP_TITLEBAR_ACTION_NONE,
  C_DESKTOP_TITLEBAR_ACTION_LOWER,
  C_DESKTOP_TITLEBAR_ACTION_MENU
} CDesktopTitlebarAction;

typedef enum
{
  /* these must be more than max CDesktopTitlebarAction */
  C_DESKTOP_TITLEBAR_SCROLL_ACTION_SHADE = 10,
  C_DESKTOP_TITLEBAR_SCROLL_ACTION_OPACITY,
  C_DESKTOP_TITLEBAR_SCROLL_ACTION_NONE
} CDesktopTitlebarScrollAction;

typedef enum
{
  C_DESKTOP_FOCUS_MODE_CLICK,
  C_DESKTOP_FOCUS_MODE_SLOPPY,
  C_DESKTOP_FOCUS_MODE_MOUSE,
} CDesktopFocusMode;

typedef enum
{
  C_DESKTOP_FOCUS_NEW_WINDOWS_SMART,
  C_DESKTOP_FOCUS_NEW_WINDOWS_STRICT,
} CDesktopFocusNewWindows;

typedef enum
{
  C_DESKTOP_VISUAL_BELL_FULLSCREEN_FLASH,
  C_DESKTOP_VISUAL_BELL_FRAME_FLASH,
} CDesktopVisualBellType;

typedef enum
{
/* All bindings before _SEPARATOR are treated as
 * "global" bindings, i.e. they work regardless of
 * Cinnamon's global state (open menus, etc...)
 */
        C_DESKTOP_MEDIA_KEY_MUTE,
        C_DESKTOP_MEDIA_KEY_MUTE_QUIET,
        C_DESKTOP_MEDIA_KEY_VOLUME_UP,
        C_DESKTOP_MEDIA_KEY_VOLUME_UP_QUIET,
        C_DESKTOP_MEDIA_KEY_VOLUME_DOWN,
        C_DESKTOP_MEDIA_KEY_VOLUME_DOWN_QUIET,
        C_DESKTOP_MEDIA_KEY_EJECT,
        C_DESKTOP_MEDIA_KEY_MEDIA,
        C_DESKTOP_MEDIA_KEY_SCREENSHOT,
        C_DESKTOP_MEDIA_KEY_WINDOW_SCREENSHOT,
        C_DESKTOP_MEDIA_KEY_PLAY,
        C_DESKTOP_MEDIA_KEY_PAUSE,
        C_DESKTOP_MEDIA_KEY_STOP,
        C_DESKTOP_MEDIA_KEY_PREVIOUS,
        C_DESKTOP_MEDIA_KEY_NEXT,
        C_DESKTOP_MEDIA_KEY_REWIND,
        C_DESKTOP_MEDIA_KEY_FORWARD,
        C_DESKTOP_MEDIA_KEY_REPEAT,
        C_DESKTOP_MEDIA_KEY_RANDOM,
        C_DESKTOP_MEDIA_KEY_AREA_SCREENSHOT,
        C_DESKTOP_MEDIA_KEY_SCREENSHOT_CLIP,
        C_DESKTOP_MEDIA_KEY_WINDOW_SCREENSHOT_CLIP,
        C_DESKTOP_MEDIA_KEY_AREA_SCREENSHOT_CLIP,

        C_DESKTOP_MEDIA_KEY_SEPARATOR,
/* The rest are normal priority - they won't trigger during
   a modal Cinnamon state
 */
        C_DESKTOP_MEDIA_KEY_TOUCHPAD,
        C_DESKTOP_MEDIA_KEY_TOUCHPAD_ON,
        C_DESKTOP_MEDIA_KEY_TOUCHPAD_OFF,
        C_DESKTOP_MEDIA_KEY_LOGOUT,
        C_DESKTOP_MEDIA_KEY_SHUTDOWN,
        C_DESKTOP_MEDIA_KEY_HOME,
        C_DESKTOP_MEDIA_KEY_CALCULATOR,
        C_DESKTOP_MEDIA_KEY_SEARCH,
        C_DESKTOP_MEDIA_KEY_EMAIL,
        C_DESKTOP_MEDIA_KEY_SCREENSAVER,
        C_DESKTOP_MEDIA_KEY_HELP,
        C_DESKTOP_MEDIA_KEY_TERMINAL,
        C_DESKTOP_MEDIA_KEY_WWW,
        C_DESKTOP_MEDIA_KEY_VIDEO_OUT,
        C_DESKTOP_MEDIA_KEY_ROTATE_VIDEO,
        C_DESKTOP_MEDIA_KEY_SCREENREADER,
        C_DESKTOP_MEDIA_KEY_ON_SCREEN_KEYBOARD,
        C_DESKTOP_MEDIA_KEY_INCREASE_TEXT,
        C_DESKTOP_MEDIA_KEY_DECREASE_TEXT,
        C_DESKTOP_MEDIA_KEY_TOGGLE_CONTRAST,
        C_DESKTOP_MEDIA_KEY_SUSPEND,
        C_DESKTOP_MEDIA_KEY_HIBERNATE,
        C_DESKTOP_MEDIA_KEY_SCREEN_BRIGHTNESS_UP,
        C_DESKTOP_MEDIA_KEY_SCREEN_BRIGHTNESS_DOWN,
        C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_UP,
        C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_DOWN,
        C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_TOGGLE,
        C_DESKTOP_MEDIA_KEY_BATTERY,

        C_DESKTOP_MEDIA_KEY_LAST
} CDesktopMediaKeyType;

__attribute__((unused)) static const char *media_keys[] = {
        [C_DESKTOP_MEDIA_KEY_MUTE] = "volume-mute",
        [C_DESKTOP_MEDIA_KEY_MUTE_QUIET] = "mute-quiet",
        [C_DESKTOP_MEDIA_KEY_VOLUME_UP] = "volume-up",
        [C_DESKTOP_MEDIA_KEY_VOLUME_UP_QUIET] = "volume-up-quiet",
        [C_DESKTOP_MEDIA_KEY_VOLUME_DOWN] = "volume-down",
        [C_DESKTOP_MEDIA_KEY_VOLUME_DOWN_QUIET] = "volume-down-quiet",
        [C_DESKTOP_MEDIA_KEY_EJECT] = "eject",
        [C_DESKTOP_MEDIA_KEY_MEDIA] = "media",
        [C_DESKTOP_MEDIA_KEY_SCREENSHOT] = "screenshot",
        [C_DESKTOP_MEDIA_KEY_WINDOW_SCREENSHOT] = "window-screenshot",
        [C_DESKTOP_MEDIA_KEY_PLAY] = "play",
        [C_DESKTOP_MEDIA_KEY_PAUSE] = "pause",
        [C_DESKTOP_MEDIA_KEY_STOP] = "stop",
        [C_DESKTOP_MEDIA_KEY_PREVIOUS] = "previous",
        [C_DESKTOP_MEDIA_KEY_NEXT] = "next",
        [C_DESKTOP_MEDIA_KEY_REWIND] = "audio-rewind",
        [C_DESKTOP_MEDIA_KEY_FORWARD] = "audio-forward",
        [C_DESKTOP_MEDIA_KEY_REPEAT] = "audio-repeat",
        [C_DESKTOP_MEDIA_KEY_RANDOM] = "audio-random",
        [C_DESKTOP_MEDIA_KEY_AREA_SCREENSHOT] = "area-screenshot",
        [C_DESKTOP_MEDIA_KEY_SCREENSHOT_CLIP] = "screenshot-clip",
        [C_DESKTOP_MEDIA_KEY_WINDOW_SCREENSHOT_CLIP] = "window-screenshot-clip",
        [C_DESKTOP_MEDIA_KEY_AREA_SCREENSHOT_CLIP] = "area-screenshot-clip",

        [C_DESKTOP_MEDIA_KEY_SEPARATOR] = "",

        [C_DESKTOP_MEDIA_KEY_TOUCHPAD] = "touchpad-toggle",
        [C_DESKTOP_MEDIA_KEY_TOUCHPAD_ON] = "touchpad-on",
        [C_DESKTOP_MEDIA_KEY_TOUCHPAD_OFF] = "touchpad-off",
        [C_DESKTOP_MEDIA_KEY_LOGOUT] = "logout",
        [C_DESKTOP_MEDIA_KEY_SHUTDOWN] = "shutdown",
        [C_DESKTOP_MEDIA_KEY_HOME] = "home",
        [C_DESKTOP_MEDIA_KEY_CALCULATOR] = "calculator",
        [C_DESKTOP_MEDIA_KEY_SEARCH] = "search",
        [C_DESKTOP_MEDIA_KEY_EMAIL] = "email",
        [C_DESKTOP_MEDIA_KEY_SCREENSAVER] = "screensaver",
        [C_DESKTOP_MEDIA_KEY_HELP] = "help",
        [C_DESKTOP_MEDIA_KEY_TERMINAL] = "terminal",
        [C_DESKTOP_MEDIA_KEY_WWW] = "www",
        [C_DESKTOP_MEDIA_KEY_VIDEO_OUT] = "video-outputs",
        [C_DESKTOP_MEDIA_KEY_ROTATE_VIDEO] = "video-rotation",
        [C_DESKTOP_MEDIA_KEY_SCREENREADER] = "screenreader",
        [C_DESKTOP_MEDIA_KEY_ON_SCREEN_KEYBOARD] = "on-screen-keyboard",
        [C_DESKTOP_MEDIA_KEY_INCREASE_TEXT] = "increase-text-size",
        [C_DESKTOP_MEDIA_KEY_DECREASE_TEXT] = "decrease-text-size",
        [C_DESKTOP_MEDIA_KEY_TOGGLE_CONTRAST] = "toggle-contrast",
        [C_DESKTOP_MEDIA_KEY_SUSPEND] = "suspend",
        [C_DESKTOP_MEDIA_KEY_HIBERNATE] = "hibernate",
        [C_DESKTOP_MEDIA_KEY_SCREEN_BRIGHTNESS_UP] = "screen-brightness-up",
        [C_DESKTOP_MEDIA_KEY_SCREEN_BRIGHTNESS_DOWN] = "screen-brightness-down",
        [C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_UP] = "kbd-brightness-up",
        [C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_DOWN] = "kbd-brightness-down",
        [C_DESKTOP_MEDIA_KEY_KEYBOARD_BRIGHTNESS_TOGGLE] = "kbd-brightness-toggle",
        [C_DESKTOP_MEDIA_KEY_BATTERY] = "battery",

        [C_DESKTOP_MEDIA_KEY_LAST] = ""
};

#endif /* __CDesktop_enums_h__ */
