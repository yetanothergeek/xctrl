/*
X11 control library, based largely on Tomas Styblo's "wmctrl".

The original wmctrl is Copyright (C) 2003 Tomas Styblo <tripie@cpan.org>
and is available from http://tripie.sweb.cz/utils/wmctrl/

This library derived in 2010 by Jeffrey Pohlmeyer <yetanothergeek@gmail.com>

The charset conversion routines were derived from Text-Unaccent-1.08/unac.c
  Copyright (C) 2000, 2001, 2002, 2003 Loic Dachary <loic@senga.org>
  Text-Unaccent is available from http://search.cpan.org/~ldachary/

The clipboard and selection routines were derived from xclip,
a command line interface to X server selections.
  Copyright (C) 2001 Kim Saunders
  Copyright (C) 2007-2008 Peter Astrand
  xclip is available from http://sourceforge.net/projects/xclip/

All source code mentioned above is released under the following terms:

This program is free software, released under the GNU General Public
License. You may redistribute and/or modify this program under the terms
of that license as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

To get a copy of the GNU General Puplic License,  write to the
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/



#ifndef XCTRL_API
# define XCTRL_API
#endif

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;


#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */


typedef struct _Geometry {
  int x;
  int y;
  unsigned int w;
  unsigned int h;
} Geometry;

/*
  Any functions that return a pointer should
  be disposed of by the caller with free()
*/

/* Charset functions */
XCTRL_API void init_charset(Bool force_utf8, char*charset);
XCTRL_API char *convert_locale(const char*src, const char*from, const char*to);
XCTRL_API char* locale_to_utf8(const char*src);
XCTRL_API char* utf8_to_locale(const char*src);

/* Window selection functions */
XCTRL_API Window *get_window_list(Display *disp, ulong*size);
XCTRL_API Window select_window(Display *disp, int button); /* -1 = any button */
XCTRL_API Window get_active_window(Display *disp);

/* Window information and manipulation functions */
XCTRL_API Bool activate_window(Display *disp, Window win, Bool switch_desk);
XCTRL_API int set_window_state(Display *disp, Window win, ulong action, const char*p1, const char*p2);
XCTRL_API Bool iconify_window(Display *disp, Window win);
XCTRL_API int close_window(Display *disp, Window win);

XCTRL_API char* get_window_class(Display *disp, Window win);
XCTRL_API char* get_window_title(Display *disp, Window win);
XCTRL_API void set_window_title(Display *disp, Window win, const char *title, char mode);

XCTRL_API int set_window_geom(Display *disp, Window win, long grav, long x, long y, long w, long h);
XCTRL_API void get_window_geom(Display *disp,  Window win, Geometry*geom);

XCTRL_API int send_window_to_desktop(Display *disp, Window win, int desktop);
XCTRL_API long get_desktop_of_window(Display *disp, Window win);

XCTRL_API ulong get_win_pid(Display *disp, Window win);
XCTRL_API char*get_client_machine(Display *disp, Window win);
XCTRL_API void send_keystrokes(Display *disp, Window win, const char*keys);

/* Desktop information and manipulation functions */
XCTRL_API int get_showing_desktop(Display *disp);
XCTRL_API int set_showing_desktop(Display *disp, ulong state);

XCTRL_API char* get_desktop_name(Display *disp, int desknum, Bool force_utf8);
XCTRL_API int get_workarea_geom(Display *disp, Geometry*geom, int desknum);

XCTRL_API int get_desktop_geom(Display *disp, int desknum, Geometry*geom);
XCTRL_API int change_geometry(Display *disp, ulong x, ulong y);
XCTRL_API int change_viewport(Display *disp, ulong x, ulong y);

XCTRL_API long get_number_of_desktops(Display *disp);
XCTRL_API int set_number_of_desktops(Display *disp, ulong n);

XCTRL_API long get_current_desktop(Display *disp);
XCTRL_API int set_current_desktop(Display *disp, ulong target);


/* Window manager information functions */
XCTRL_API Window supporting_wm_check(Display *disp);
XCTRL_API char* get_wm_name(Display *disp);
XCTRL_API char* get_wm_class(Display *disp);
XCTRL_API ulong get_wm_pid(Display *disp);

XCTRL_API void set_selection(Display*dpy, char kind, char *sel_buf, Bool utf8);
XCTRL_API uchar* get_selection(Display* dpy, char kind, Bool utf8);

