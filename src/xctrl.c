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


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>

#include <iconv.h>
#include <errno.h>
#include "xctrl.h"


#define DefRootWin DefaultRootWindow(disp)
#define GetUTF8Atom() XInternAtom(disp, "UTF8_STRING", False)
#define sfree(p) if (p) { free(p); }

#define DEFAULT_CHARSET "ISO_8859-1"

XCTRL_API char* default_charset=DEFAULT_CHARSET; 

XCTRL_API Bool envir_utf8;


XCTRL_API char* convert_locale(const char*asrc,const char*from, const char*to)
{
  iconv_t cd;
  size_t out_remain;
  size_t out_size;
  char* psrc=(char*)asrc;
  char* out_base;
  char* pdst;
  Bool from_utf16 = (strcmp("UTF-16BE", from)==0)||(strcmp("UTF-16", from)==0);
  const char space[] = { 0x00, 0x20 };
  size_t srclen=asrc?strlen(asrc):0;
  out_size = srclen > 0 ? srclen : 1024;
  out_base = (char*)malloc(out_size + 1);
  if(!out_base) { return NULL; }
  out_remain = out_size;
  pdst = out_base;
  if ((cd = iconv_open(to, from)) == (iconv_t)-1) {
    free(out_base);
    return NULL;
  }
  do {
    if (iconv(cd, &psrc, &srclen, &pdst, &out_remain) == (size_t)-1) {
      switch(errno) {
        case EILSEQ: {
          if(from_utf16) {
            char* tmp = (char*)space;
            size_t tmp_length = 2;
            if(iconv(cd, &tmp, &tmp_length, &pdst, &out_remain) == (size_t)-1) {
              if (errno != E2BIG) {
                free(out_base);
                return NULL; 
              } else { 
                /* fall thru to E2BIG below */
              }
            } else {  /* The bad char was replaced with SPACE, so skip it. */
              psrc += 2;
              srclen -= 2;
              break;
            }
          } else { 
            free(out_base);
            return NULL;
          }
        }
        case E2BIG: {
          int length = pdst - out_base;
          out_size *= 2;
          out_base = (char*)realloc(out_base, out_size + 1);
          if (!out_base) { return NULL; }
          pdst = out_base + length;
          out_remain = out_size - length;
          break;
        }
        default: {
          free(out_base);
          return NULL;
        }
      }
    }
  } while(srclen > 0);
  iconv_close(cd);
  pdst[0]='\0';
  return out_base;
}



XCTRL_API char *locale_to_utf8(const char*src) {
  return convert_locale( src, default_charset, "UTF-8");
}



XCTRL_API char *utf8_to_locale(const char*src) {
  return convert_locale(src, "UTF-8", default_charset);
}



static char *ascii_strup(const char*src)
{
  if (src) {
    char*tmp=strdup(src);
    char*p;
    for (p=tmp; *p; p++) { if ((signed char)(*p) > 0 ) { *p=toupper(*p); } }
    return tmp;
  } else {
    return NULL;
  }
}



static Bool is_envar_utf8(const char*envar)
{
  char*enval=getenv(envar);
  if (enval) {
    Bool rv;
    enval=ascii_strup(enval);
    rv=strstr(enval, "UTF8") || strstr(enval, "UTF-8");
    sfree(enval);
    return rv;
  } else {
  return False;
  }
}




XCTRL_API void init_charset(Bool force_utf8, char*charset)
{
  envir_utf8 = force_utf8?True:is_envar_utf8("LANG") || is_envar_utf8("LC_CTYPE");
  default_charset = charset ? charset : getenv("CHARSET") ? getenv("CHARSET") : DEFAULT_CHARSET;
}



static Bool client_msg(Display *disp, Window win, char *msg, ulong d0, ulong d1, ulong d2, ulong d3, ulong d4) {
  XEvent event;
  long mask = SubstructureRedirectMask | SubstructureNotifyMask;
  event.type = ClientMessage;
  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, msg, False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = d0;
  event.xclient.data.l[1] = d1;
  event.xclient.data.l[2] = d2;
  event.xclient.data.l[3] = d3;
  event.xclient.data.l[4] = d4;
  return XSendEvent(disp, DefRootWin, False, mask, &event)?True:False;
}



static char *get_output_str(char *str, Bool is_utf8) {
  char *out;
  if (!str) { return NULL; }
  if (envir_utf8) {
    if (is_utf8) {
      out = strdup(str);
    } else {
      out=locale_to_utf8(str);
      if (!out) { out = strdup(str); }
    }
  } else {
    if (is_utf8) {
      out = utf8_to_locale(str);
      if (!out) { out = strdup(str); }
    } else {
      out = strdup(str);
    }
  }
  return out;
}



#define get_uprop(wn,pn,sz) (ulong*)get_prop(disp, wn, XA_CARDINAL, pn, sz)

static char *get_prop(Display*d, Window w, Atom type, const char*name, ulong*size) {
  Atom a=XInternAtom(d, name, False);
  Atom ret_type;
  int format;
  ulong nitems;
  ulong after;
  ulong tmp_size;
  unsigned char *retp;
  char *ret;
# define MAX_PROP_VAL_LEN 4096
# define MPVL4 MAX_PROP_VAL_LEN / 4
  /*
    MAX_PROP_VAL_LEN / 4 explanation (XGetWindowProperty manpage):
   long_length = Specifies the length in 32-bit multiples of the data to be retrieved.
  */
  if (XGetWindowProperty(d,w,a,0,MPVL4,False,type,&ret_type,&format,&nitems,&after,&retp) != Success) { return NULL; }
  if (ret_type != type) {
    XFree(retp);
    return NULL;
  }
  /* null terminate the result to make string handling easier */
  tmp_size = (format / 8) * nitems;
  ret = (char*)calloc(tmp_size + 1, 1);
  memcpy(ret, retp, tmp_size);
  ret[tmp_size] = '\0';
  if (size) {
    *size = tmp_size;
  }
  XFree(retp);
  return ret;
}



static char* get_prop_utf8(Display *disp, Window win, const char*what)
{
  Bool name_is_utf8 = True;
  char*result=NULL;
  char*tmp=get_prop(disp, win, GetUTF8Atom(), what, NULL);
  if (!tmp) {
    name_is_utf8 = False;
    tmp = get_prop(disp, win, XA_STRING, what, NULL);
  }
  result=get_output_str(tmp, name_is_utf8);
  sfree(tmp);
  return result;
}


/*
  Save the value of p, free p, and return the 
  saved value, or return ifnull if p is NULL.
*/
static ulong ptr_to_ulong(ulong*p, ulong ifnull)
{
  if (p) {
    ulong rv=*p;
    free(p);
    return rv;
  } else {
    return ifnull;
  }
}



XCTRL_API Window supporting_wm_check(Display *disp)
{
  Window *p=(Window*)get_prop(disp, DefRootWin, XA_WINDOW, "_NET_SUPPORTING_WM_CHECK", NULL);
  if (!p) {
    p=(Window*)get_prop(disp, DefRootWin, XA_CARDINAL, "_WIN_SUPPORTING_WM_CHECK", NULL);
  }
  return (Window)ptr_to_ulong((ulong*)p,0);
}



XCTRL_API char* get_wm_name(Display *disp)
{
  Window win = supporting_wm_check(disp);
  return win?get_prop_utf8(disp, win, "_NET_WM_NAME"):NULL;
}



XCTRL_API char* get_wm_class(Display *disp)
{
  Window win = supporting_wm_check(disp);
  return win?get_prop_utf8(disp, win, "WM_CLASS"):NULL;
}



XCTRL_API ulong get_wm_pid(Display *disp)
{
  Window win = supporting_wm_check(disp);
  if (win) {
    ulong *wm_pid = get_uprop(win, "_NET_WM_PID", NULL);
    return ptr_to_ulong(wm_pid, 0);
  } else {
    return 0;
  }
}



XCTRL_API int get_showing_desktop(Display *disp)
{
  ulong *p=get_uprop(DefRootWin, "_NET_SHOWING_DESKTOP", NULL);
  return ptr_to_ulong(p,-1);
}



XCTRL_API int set_showing_desktop(Display *disp, ulong state)
{
  return client_msg(disp, DefRootWin, "_NET_SHOWING_DESKTOP", state, 0, 0, 0, 0);
}



XCTRL_API int change_viewport(Display *disp, ulong x, ulong y) {
  return client_msg(disp, DefRootWin, "_NET_DESKTOP_VIEWPORT", x, y, 0, 0, 0);
}



XCTRL_API int change_geometry (Display *disp, ulong x, ulong y) {
  return client_msg(disp, DefRootWin, "_NET_DESKTOP_GEOMETRY", x, y, 0, 0, 0);
}



XCTRL_API int set_number_of_desktops(Display *disp, ulong n)
{
  return client_msg(disp, DefRootWin, "_NET_NUMBER_OF_DESKTOPS", n, 0, 0, 0, 0);
}



XCTRL_API long get_number_of_desktops(Display *disp)
{
  ulong *p;
  p = get_uprop(DefRootWin, "_NET_NUMBER_OF_DESKTOPS", NULL);
  if (!p) {
    p = get_uprop(DefRootWin, "_WIN_WORKSPACE_COUNT", NULL);
  }
  return ptr_to_ulong(p,-1);
}



XCTRL_API int set_current_desktop(Display *disp, ulong target) {
  return client_msg(disp, DefRootWin, "_NET_CURRENT_DESKTOP", target, 0, 0, 0, 0);
}



XCTRL_API long get_current_desktop(Display *disp)
{
  ulong *p;
  p = get_uprop(DefRootWin, "_NET_CURRENT_DESKTOP", NULL);
  if (!p) {
    p = get_uprop(DefRootWin, "_WIN_WORKSPACE", NULL);
  }
  return (signed long)ptr_to_ulong(p,-1);
}



static void replace_prop(Display *disp, Window win, Atom what, Atom type, char*newval)
{
  XChangeProperty(disp,win,what,type,8,PropModeReplace,(unsigned char*)newval,strlen(newval));
}



XCTRL_API void set_window_title(Display *disp, Window win, const char *title, char mode) {
  char *title_utf8;
  char *title_local;
  Atom utf8_atom = GetUTF8Atom();
  if (envir_utf8) {
    title_utf8 = strdup(title);
    title_local = NULL;
  } else {
    title_utf8 = locale_to_utf8(title);
    if (!title_utf8) { title_utf8 = strdup(title); }
    title_local = strdup(title);
  }
  if (mode == 'T' || mode == 'N') {
    if (title_local) {
      replace_prop(disp, win, XA_WM_NAME, XA_STRING,title_local);
    } else {
      XDeleteProperty(disp, win, XA_WM_NAME);
    }
    replace_prop(disp, win, XInternAtom(disp,"_NET_WM_NAME",False), utf8_atom, title_utf8);
  }
  if (mode == 'T' || mode == 'I') {
    if (title_local) {
      replace_prop(disp, win, XA_WM_ICON_NAME, XA_STRING, title_local);
    } else {
      XDeleteProperty(disp, win, XA_WM_ICON_NAME);
    }
    replace_prop(disp, win, XInternAtom(disp, "_NET_WM_ICON_NAME", False), utf8_atom, title_utf8);
  }
  sfree(title_utf8);
  sfree(title_local);
}



XCTRL_API int send_window_to_desktop(Display *disp, Window win, int desktop) {
  return client_msg(disp, win, "_NET_WM_DESKTOP", (ulong)desktop, 0, 0, 0, 0);
}



XCTRL_API Bool activate_window(Display *disp, Window win, Bool switch_desk) {
  if (switch_desk) {
    long desktop = get_desktop_of_window(disp,win);
    if (desktop>=0) { 
      if (desktop!=get_current_desktop(disp)) {
        set_current_desktop(disp, desktop);
      }
    }
  }
  client_msg(disp, win, "_NET_ACTIVE_WINDOW", 2, 0, 0, 0, 0);
  XSetInputFocus(disp, win, RevertToNone, CurrentTime);
  XMapRaised(disp, win);
  return True;
}



XCTRL_API Bool iconify_window(Display *disp, Window win)
{
  return XIconifyWindow(disp, win, DefaultScreen(disp))?True:False;
}



XCTRL_API Window get_active_window(Display *disp) {
  Window*win = (Window*)get_prop(disp, DefRootWin, XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL);
  return (Window) ptr_to_ulong((ulong*)win,0);
}



XCTRL_API int close_window(Display *disp, Window win) {
  return client_msg(disp, win, "_NET_CLOSE_WINDOW", 0, 0, 0, 0, 0);
}



static ulong wm_state_atom(Display *disp, const char*prop) {
  Atom atom=0;
  char tmp_prop[64];
  char*p;
  memset(tmp_prop,0,sizeof(tmp_prop));
  strcpy(tmp_prop, "_NET_WM_STATE_");
  strncat(tmp_prop,prop,(sizeof(tmp_prop)-strlen(tmp_prop))-1);
  for (p=tmp_prop; *p; p++) {
    if (((signed char)*p)>0) { *p=toupper(*p); }
  }
  atom = XInternAtom(disp, tmp_prop, False);
  return (ulong)atom;
}



XCTRL_API int set_window_state(Display *disp, Window win, ulong action, const char*p1, const char*p2)
{  
  ulong xa_prop1=p1?wm_state_atom(disp,p1):0;
  ulong xa_prop2=p2?wm_state_atom(disp,p2):0;
  return client_msg(disp,win,"_NET_WM_STATE",action,xa_prop1,xa_prop2, 0, 0);
}



static Bool wm_supports (Display *disp, const char *prop) {
  Atom xa_prop = XInternAtom(disp, prop, False);
  ulong size;
  int i;
  Atom *list=(Atom*)get_prop(disp, DefRootWin, XA_ATOM, "_NET_SUPPORTED", &size);
  if (!list) { return False; }
  for (i = 0; i < size / sizeof(Atom); i++) {
    if (list[i] == xa_prop) {
      free(list);
      return True;
    }
  }
  free(list);
  return False;
}



XCTRL_API void get_window_geom(Display *disp, Window win, Geometry*geom)
{
  int x, y;
  unsigned int bw, depth;
  Window root;
  memset(geom,0,sizeof(Geometry));
  XGetGeometry(disp, win, &root, &x, &y, &geom->w, &geom->h, &bw, &depth);
  XTranslateCoordinates (disp, win, root, x, y, &geom->x, &geom->y, &root);
}



XCTRL_API int set_window_geom(Display *disp, Window win, long grav, long x, long y, long w, long h)
{
  ulong grflags = grav;
  if (x != -1) grflags |= (1 << 8);
  if (y != -1) grflags |= (1 << 9);
  if (w != -1) grflags |= (1 << 10);
  if (h != -1) grflags |= (1 << 11);
  if (wm_supports(disp, "_NET_MOVERESIZE_WINDOW")) {
    return client_msg( disp, win, "_NET_MOVERESIZE_WINDOW", 
                       grflags, (ulong)x, (ulong)y, (ulong)w, (ulong)h);
  } else {
    if ((w < 1 || h < 1) && (x >= 0 && y >= 0)) {
      XMoveWindow(disp, win, x, y);
    } else if ((x < 0 || y < 0) && (w >= 1 && h >= -1)) {
      XResizeWindow(disp, win, w, h);
    } else if (x >= 0 && y >= 0 && w >= 1 && h >= 1) {
      XMoveResizeWindow(disp, win, x, y, w, h);
    }
    return True;
  }
}



XCTRL_API Window *get_window_list(Display *disp, ulong*size)
{
  Window *client_list;
  client_list = (Window*)get_prop(disp, DefRootWin, XA_WINDOW, "_NET_CLIENT_LIST", size);
  if (!client_list) {
    client_list = (Window*)get_prop(disp, DefRootWin, XA_CARDINAL, "_WIN_CLIENT_LIST", size);
  }
  return client_list;
}



XCTRL_API char *get_window_class (Display *disp, Window win)
{
  char *class_utf8;
  char *wm_class;
  ulong size;
  wm_class = get_prop(disp, win, XA_STRING, "WM_CLASS", &size);
  if (wm_class) {
    char *p_0 = strchr(wm_class, '\0');
    if (wm_class + size - 1 > p_0) { *(p_0) = '.'; }
    class_utf8 = locale_to_utf8(wm_class);
  } else {
    class_utf8 = NULL;
  }
  sfree(wm_class);
  return class_utf8;
}



XCTRL_API char *get_window_title (Display *disp, Window win)
{
  char *wm_name = get_prop(disp, win, GetUTF8Atom(), "_NET_WM_NAME", NULL);
  if (wm_name) {
    return wm_name;
  } else {
    char *title_utf8 = NULL;
    wm_name = get_prop(disp, win, XA_STRING, "WM_NAME", NULL);
    if (wm_name) {
      title_utf8 = locale_to_utf8(wm_name);
      free(wm_name);
    } else {
      title_utf8 = NULL;
    }
    return title_utf8;
  }
}



static void pass_click_to_client(Display *disp, Window root, XEvent *event, int mask, Cursor cursor)
{
  usleep(1000);
  XSync(disp,False);
  event->xbutton.window=event->xbutton.subwindow;
  XSendEvent(disp, event->xbutton.subwindow, True, mask, event);
  usleep(1000);
  XSync(disp,False);
}


#define GrabMouse() ( \
  XGrabPointer(disp,root,False,ButtonPressMask|ButtonReleaseMask, \
    GrabModeSync,GrabModeAsync,root,cursor,CurrentTime) == GrabSuccess )


/* Routine to let user select a window using the mouse adapted from xfree86. */
XCTRL_API Window select_window(Display *disp, int button)
{
  Cursor cursor;
  XEvent event;
  Window target_win = None;
  Window root = DefaultRootWindow(disp);
  int buttons = 0;
  int dumi;
  unsigned int dum;
  cursor = XCreateFontCursor(disp, XC_crosshair);
  if (!GrabMouse()) {
    XFreeCursor(disp,cursor);
    return 0;
  }
  while ((target_win == None) || (buttons != 0)) {
    XAllowEvents(disp, SyncPointer, CurrentTime);
    XWindowEvent(disp, root, ButtonPressMask|ButtonReleaseMask, &event);
    switch (event.type) {
      case ButtonPress:
        if ((button==-1)||(event.xbutton.button==button)) {
          if (target_win == None) {
            target_win = event.xbutton.subwindow; /* window selected */
            if (target_win == None) target_win = root;
          }
        } else {
          pass_click_to_client(disp,root,&event,ButtonPressMask,cursor);
        }
        buttons++;
        break;
      case ButtonRelease:
        if (buttons > 0) { buttons--; }
        if ((button!=-1)&&(event.xbutton.button!=button)) {
          pass_click_to_client(disp,root,&event,ButtonReleaseMask,cursor);
        }
        break;
    }
  }
  XUngrabPointer(disp, CurrentTime);
  if (XGetGeometry(disp,target_win,&root,&dumi,&dumi,&dum,&dum,&dum,&dum)&&target_win!=root) {
    target_win = XmuClientWindow(disp, target_win);
  }
  return(target_win);
}



XCTRL_API char* get_desktop_name(Display *disp, int desknum, Bool force_utf8)
{
  char *name_list = NULL;
  ulong name_list_size = 0;
  char *name = NULL;
  Bool names_are_utf8 = True;
  Window root = DefRootWin;
  char*rv=NULL;
  if (desknum<0) { return NULL; }
  if (force_utf8) {
    name_list = get_prop(disp, root, GetUTF8Atom(), "_NET_DESKTOP_NAMES", &name_list_size);
  }
  if (!name_list) {
    names_are_utf8 = False;
    name_list = get_prop(disp, root, XA_STRING, "_WIN_WORKSPACE_NAMES", &name_list_size);
  }
  if (name_list) {
    if (desknum>0) { 
      int id=1;
      char*p=name_list;
      do {
        p=strchr(p,'\0')+1;
        if (id==desknum) {
          name=p;
          break;
        }
        id++;
      } while (p<(name_list+name_list_size));
    } else { name=name_list; }
    if (name && *name) { rv = get_output_str(name, names_are_utf8); }
    free(name_list);
  }
  return rv;
}



XCTRL_API int get_workarea_geom(Display *disp, Geometry*geom, int desknum)
{
  ulong *wkarea = NULL;
  ulong wkarea_size = 0;
  int rv=0;
  if (desknum<0) { return 0; }
  if (get_number_of_desktops(disp)<=desknum) { return 0; }
  wkarea = get_uprop(DefRootWin, "_NET_WORKAREA", &wkarea_size);
  if (!wkarea) {
    wkarea = get_uprop(DefRootWin, "_WIN_WORKAREA", &wkarea_size);
  }
  if (wkarea && wkarea_size > 0) {
    if (wkarea_size == (4*sizeof(*wkarea))) {
      geom->x=wkarea[0];
      geom->y=wkarea[1];
      geom->w=wkarea[2];          
      geom->h=wkarea[3];
      rv=1;
    } else {
      if (desknum < wkarea_size / sizeof(*wkarea) / 4) {
        geom->y=wkarea[desknum*4];
        geom->x=wkarea[desknum*4+1];
        geom->w=wkarea[desknum*4+2];
        geom->h=wkarea[desknum*4+3];
        rv=1;
      }
    }
    sfree(wkarea);
  }
  return rv;
}



XCTRL_API int get_desktop_geom(Display *disp, int desknum, Geometry*geom)
{
  ulong *desk_geom   =  NULL;
  ulong geom_size = 0;
  ulong *vport  = NULL;
  ulong vport_size = 0;
  int rv=0;
  if (desknum<0) { return 0; }
  if (get_number_of_desktops(disp)<=desknum) { return 0; }
  desk_geom = get_uprop(DefRootWin, "_NET_DESKTOP_GEOMETRY", &geom_size);
  vport = get_uprop(DefRootWin, "_NET_DESKTOP_VIEWPORT", &vport_size);
  if (desk_geom && geom_size > 0) {
    if (geom_size == 2 * sizeof(*desk_geom)) {
      geom->w=desk_geom[0];
      geom->h=desk_geom[1];
    } else {
      geom->w=desk_geom[desknum*2];
      geom->h=desk_geom[desknum*2+1];
    }
    rv=1;
  }
  if (vport && vport_size > 0) {
    if (vport_size == 2 * sizeof(*vport)) {
      geom->x=vport[0];
      geom->y=vport[1];
    } else {
      if (desknum < vport_size / sizeof(*vport) / 2) {
        geom->x=vport[desknum*2];
        geom->y=vport[desknum*2+1];
      }
    }
    rv=1;
  }
  sfree(desk_geom);
  sfree(vport);
  return rv;
}



XCTRL_API long get_desktop_of_window(Display *disp, Window win)
{
  ulong *p = get_uprop(win, "_NET_WM_DESKTOP", NULL);
  if (!p) {
    p = get_uprop(win, "_WIN_WORKSPACE", NULL);
  }
  return (signed long)ptr_to_ulong(p,-2);
}



XCTRL_API ulong get_win_pid(Display *disp, Window win)
{
  ulong *p=get_uprop(win, "_NET_WM_PID", NULL);
  return ptr_to_ulong(p,0);
}


XCTRL_API char*get_client_machine(Display *disp, Window win)
{
  return get_prop(disp, win, XA_STRING, "WM_CLIENT_MACHINE", NULL);
}



/*
  Send "fake" keystroke events to an X window.
  Adapted from the (public domain) example by by Adam Pierce --
    http://www.doctort.org/adam/nerd-notes/x11-fake-keypress-event.html
*/
XCTRL_API void send_keystrokes(Display *disp, Window win, const char*keys)
{ 
  Bool escaped=0;
  const char* numkeys_upper="~!@#$%^&*()_+|";
  const char* numkeys_lower="`1234567890-=\\";
  int navkeys[]={XK_Insert,XK_End,XK_Down,XK_Page_Down,XK_Left,-1,XK_Right,XK_Home,XK_Up,XK_Page_Up};
  int funkeys[]={XK_F1,XK_F2,XK_F3,XK_F4,XK_F5,XK_F6,XK_F7,XK_F8,XK_F9,XK_F10,XK_F11,XK_F12};
  unsigned const char*p;
  char*n;
  XEvent ev;
  memset(&ev.xkey,0,sizeof(XKeyEvent));
  ev.xkey.subwindow=None;
  ev.xkey.serial=1;
  ev.xkey.display=disp;
  ev.xkey.window=win;
  ev.xkey.root=DefRootWin;
  ev.xkey.same_screen=1;
  for (p=(unsigned const char*)keys; *p; p++) {
    int c;
    switch (*p) {
      case '\n':
        c=XK_Return;
        break;
      case '\t':
        c=XK_Tab;
        break;
      case '\033':
        c=XK_Escape;
        break;
      case '\b':
        c=XK_BackSpace;
        break;
      case '%':
        if (!escaped) {
          escaped=True;
          continue;
        } else { 
          c=*p;
          break;
        }
      case '^':
        if (!escaped) {
          ev.xkey.state|=ControlMask;
          continue;
        }
      case '~':
        if (!escaped) {
          ev.xkey.state|=Mod1Mask;
          continue;
        }
      case '+':
        if (!escaped) {
          ev.xkey.state|=ShiftMask;
          continue;
        }
      case 'f':
      case 'F': {
        int f;
        if (escaped && p[1] && strchr("01", p[1]) && (p[2]>='0') && (p[2]<='9')) {
          char num[3]={0,0,0};
          if (p[1]=='1') { 
            strncpy(num,(char*)p+1,2);
          } else {
            num[0]=p[2];
          }
          f=atoi(num);
          c= ((f>0)&&(f<13)) ? funkeys[f-1] : *p;
          p+=2;
        } else {
          c=*p;
        }
        break;
      }
      case '.':
        c=escaped?XK_Delete:*p;
        break;
      default:
      c=*p;
    }
    n=strchr(numkeys_upper,c);
    if (n) {
      c=numkeys_lower[n-numkeys_upper];
      ev.xkey.state|=ShiftMask;
    } else {
      if (escaped && (c>='0') && (c<='9') && (c!='5')) {
        c=navkeys[c-48];
      } else {
        ev.xkey.state|=isupper(c)?ShiftMask:0;
      }
    }
    ev.xkey.keycode=XKeysymToKeycode(disp,c);
    ev.xkey.type=KeyPress;
    XSendEvent(disp, win, True, KeyPressMask,&ev);
    usleep(1000);
    XSync(disp, False);
    ev.xkey.time=CurrentTime;
    ev.xkey.type=KeyRelease;
    XSendEvent(disp, win, True, KeyPressMask,&ev);
    usleep(1000);
    XSync(disp, False);
    ev.xkey.state=0;
    escaped=False;
  }
}

/*********************************************************************/
/* * * * * * * * * Clipboard and selection functions * * * * * * * * */
/*********************************************************************/
#include <X11/Xmu/Atoms.h>
/* xcout() contexts */
#define XCLIB_XCOUT_NONE  0  /* no context */
#define XCLIB_XCOUT_SENTCONVSEL  1  /* sent a request */
#define XCLIB_XCOUT_INCR  2  /* in an incr loop */
#define XCLIB_XCOUT_FALLBACK  3  /* UTF8_STRING failed, need fallback to XA_STRING */

/* xcin() contexts */
#define XCLIB_XCIN_NONE    0
#define XCLIB_XCIN_INCR    2

/* Retrieves the contents of a selections. Arguments are:
 *
 * A display that has been opened.
 * A window
 * An event to process
 * The selection to return
 * The target(UTF8_STRING or XA_STRING) to return 
 * A pointer to a char array to put the selection into.
 * A pointer to a long to record the length of the char array
 * A pointer to an int to record the context in which to process the event
 * Returns ONE if retrieval of selection data is complete, or ZERO otherwise.
 */
static int xcout(Display*dpy, Window win, XEvent evt, Atom sel, Atom trg, uchar**txt, ulong*len, uint*ctx)
{
    static Atom prop; /* for other windows to put their selection into */
    static Atom inc;
    Atom prop_type;
    Atom atomUTF8String;
    int prop_fmt;
    uchar *buffer;  /* buffer for XGetWindowProperty to dump data into */
    ulong prop_size;
    ulong prop_items;
    uchar *ltxt = *txt;   /* local buffer of text to return */

    if (!prop) { prop = XInternAtom(dpy, "XCLIP_OUT", False); }
    if (!inc) { inc = XInternAtom(dpy, "INCR", False); }

    switch (*ctx) {
      case XCLIB_XCOUT_NONE: { /* there is no context, do an XConvertSelection() */
        if (*len > 0) {   /* initialise return length to 0 */
          free(*txt);
          *len = 0;
        }
        XConvertSelection(dpy, sel, trg, prop, win, CurrentTime);   /* send selection request */
        *ctx = XCLIB_XCOUT_SENTCONVSEL;
        return (0);
      }
      case XCLIB_XCOUT_SENTCONVSEL: {
        atomUTF8String = XInternAtom(dpy, "UTF8_STRING", False);
        if (evt.type != SelectionNotify)
            return (0);

        /* fallback to XA_STRING when UTF8_STRING failed */
        if (trg == atomUTF8String && evt.xselection.property == None) {
            *ctx = XCLIB_XCOUT_FALLBACK;
            return (0);
        }

        /* find the size and format of the data in property */
        XGetWindowProperty( dpy, win, prop, 0, 0, False, AnyPropertyType, 
                              &prop_type, &prop_fmt, &prop_items, &prop_size, &buffer );
        XFree(buffer);

        if (prop_type == inc) {
          XDeleteProperty(dpy, win, prop);  /* start INCR mechanism by deleting property */
          XFlush(dpy);
          *ctx = XCLIB_XCOUT_INCR;
          return (0);
        }

        /* if not incr, and not format == 8, it's nothing we understand anyway. */
        if (prop_fmt != 8) {
          *ctx = XCLIB_XCOUT_NONE;
          return (0);
        }

        /* not using INCR mechanism, just read the property */
        XGetWindowProperty( dpy, win, prop, 0, (long) prop_size, False, AnyPropertyType,
                              &prop_type, &prop_fmt, &prop_items, &prop_size, &buffer );

        /* finished with property, delete it */
        XDeleteProperty(dpy, win, prop);

        /* copy the buffer to the pointer for returned data */
        ltxt = (uchar*) malloc(prop_items);
        memcpy(ltxt, buffer, prop_items);

        /* set the length of the returned data */
        *len = prop_items;
        *txt = ltxt;

        /* free the buffer */
        XFree(buffer);

        *ctx = XCLIB_XCOUT_NONE;

        /* complete contents of selection fetched, return 1 */
        return (1);
      }
      case XCLIB_XCOUT_INCR: {
        /* To use the INCR method, we delete the property with the selection in it, 
           wait for an event indicating that the property has been created,
           then read it, delete it, etc. */

        if (evt.type != PropertyNotify) { return (0); }
        if (evt.xproperty.state != PropertyNewValue) { return (0); }

        /* check size and format of the property */
        XGetWindowProperty( dpy, win, prop, 0, 0, False, AnyPropertyType,
                              &prop_type, &prop_fmt, &prop_items, &prop_size, &buffer );

        if (prop_fmt != 8) {
          /* property is not text, deleting tells other client to send next property */
          XFree(buffer);
          XDeleteProperty(dpy, win, prop);
          return (0);
        }

        if (prop_size == 0) { /* no more data, exit from loop */
          XFree(buffer);
          XDeleteProperty(dpy, win, prop);
          *ctx = XCLIB_XCOUT_NONE;
          return (1); /* INCR transfer completed */
        }
        XFree(buffer);

        /* if we have come this far, the propery contains text, and we know the size. */
        XGetWindowProperty( dpy, win, prop, 0, (long) prop_size, False, AnyPropertyType,
                              &prop_type, &prop_fmt, &prop_items, &prop_size, &buffer );
         /* allocate memory to accommodate data in *txt */
        if (*len == 0) {
          *len = prop_items;
          ltxt = (uchar*) malloc(*len);
        } else {
          *len += prop_items;
          ltxt = (uchar*) realloc(ltxt, *len);
        }
        memcpy(&ltxt[*len - prop_items], buffer, prop_items); /* add data to ltxt */

        *txt = ltxt;
        XFree(buffer);
        XDeleteProperty(dpy, win, prop); /* delete property to get the next item */
        XFlush(dpy);
        return (0);
      }
    }
    return (0);
}

/* Put data into a selection, in response to a SelecionRequest event from
 * another window (and any subsequent events relating to an INCR transfer).
 *
 * Arguments are:
 *   A display
 *   A window
 *   The event to respond to
 *   A pointer to an Atom. This gets set to the property nominated by the other
 *     app in it's SelectionRequest. Things are likely to break if you change the
 *     value of this yourself.
 *   The target (UTF8_STRING or XA_STRING) to respond to
 *   A pointer to an array of chars to read selection data from.
 *   The length of the array of chars.
 *   In case of an INCR transfer, the position within the array of chars that's being processed.
 *   The context that event is the be processed within.
 */
static int xcin( Display*dpy, Window*win, XEvent evt, Atom*prop, 
                   Atom trg, uchar*txt, ulong len, ulong*pos, uint *ctx )
{
  ulong chunk_len;  /* length of current chunk (for incr transfers only) */
  XEvent resp;      /* response to event */
  static Atom inc;
  static Atom targets;
  static long chunk_size;
  if (!targets) { targets = XInternAtom(dpy, "TARGETS", False); }
  if (!inc) { inc = XInternAtom(dpy, "INCR", False); }
  /* Treat selections larger than 1/4 of the max request size as "large" per ICCCM sect. 2.5 */
  if (!chunk_size) {
    chunk_size = XExtendedMaxRequestSize(dpy) / 4;
    if (!chunk_size) { chunk_size = XMaxRequestSize(dpy) / 4; }
  }
  switch (*ctx) {
    case XCLIB_XCIN_NONE: {
      if (evt.type != SelectionRequest) { return (0); }
      *win = evt.xselectionrequest.requestor;
      *prop = evt.xselectionrequest.property;
      *pos = 0;
      if (evt.xselectionrequest.target == targets) {   /* put the data into an property */
        Atom types[2];
        int size=(int)(sizeof(types)/sizeof(Atom));
        types[0]=targets;
        types[1]=trg;
        /* send data all at once (not using INCR) */
        XChangeProperty(dpy,*win,*prop,XA_ATOM,32,PropModeReplace,(uchar*)types,size);
      } else if (len > chunk_size) {
        XChangeProperty(dpy,*win,*prop,inc,32,PropModeReplace,0,0); /* send INCR response */
        /* With INCR, we need to know when requestor window changes/deletes properties */
        XSelectInput(dpy, *win, PropertyChangeMask);
        *ctx = XCLIB_XCIN_INCR;
      } else {
        XChangeProperty(dpy,*win,*prop,trg,8,PropModeReplace,(uchar*)txt,(int)len); /* All, not INCR */
      }

      /* FIXME? According to ICCCM section 2.5, we should confirm that X ChangeProperty
         succeeded without any Alloc errors before replying with SelectionNotify. 
         However, doing so would require an error handler which modifies a global
         variable, plus doing XSync after each X ChangeProperty. */
      resp.xselection.property = *prop;
      resp.xselection.type = SelectionNotify;
      resp.xselection.display = evt.xselectionrequest.display;
      resp.xselection.requestor = *win;
      resp.xselection.selection = evt.xselectionrequest.selection;
      resp.xselection.target = evt.xselectionrequest.target;
      resp.xselection.time = evt.xselectionrequest.time;
      XSendEvent(dpy, evt.xselectionrequest.requestor, 0, 0, &resp); /* send response event */
      XFlush(dpy);
      return (len > chunk_size) ? 0 : 1; /* if data sent all at once, transfer is complete. */
      break;
    }
    case XCLIB_XCIN_INCR: { 
      if (evt.type != PropertyNotify) { return (0); } /* ignore non-property events */
      if (evt.xproperty.state != PropertyDelete) { return (0); }   /* only interest in deleted props */
      chunk_len = chunk_size; /* set length to max size */
      /* if a max-sized chunk length would extend past end, set length to remaining txt length */
      if ((*pos + chunk_len) > len) { chunk_len = len - *pos; }

      /* if start of chunk is beyond end of txt, then we've sent all data, so set length to zero */
      if (*pos > len) { chunk_len = 0; }
      if (chunk_len) {  /* put chunk into property */
        XChangeProperty(dpy,*win,*prop,trg,8,PropModeReplace,&txt[*pos],(int)chunk_len);
      } else {
        XChangeProperty(dpy,*win,*prop,trg,8,PropModeReplace,0,0); /* empty prop shows we're done */
      }
      XFlush(dpy);
      if (!chunk_len) { *ctx = XCLIB_XCIN_NONE; }   /* all data is sent, break out of the loop */
      *pos += chunk_size;
      return (chunk_len==0) ? 1 : 0; /* chunk_len == 0 means we finished the transfer. */
      break;
    }
  }
  return (0);
}



static Atom selarg_to_seltype(Display*dpy, char arg)
{
  switch (arg) {
    case 'p': return XA_PRIMARY;
    case 's': return XA_SECONDARY;
    case 'b': return XA_STRING;
    case 'c': return XA_CLIPBOARD(dpy);
    default:return XA_PRIMARY;
  }
}


static Window make_selection_window(Display*dpy)
{
  Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
  XSelectInput(dpy, win, PropertyChangeMask);
  return win;
}



XCTRL_API void set_selection(Display*dpy, char kind, char *sel_buf, Bool utf8)
{
  Atom seltype=selarg_to_seltype(dpy,kind);
  long sel_len=strlen(sel_buf);
  if (seltype == XA_STRING) {
    XStoreBuffer(dpy, (char*)sel_buf, (int)sel_len, 0);
  } else {
    XEvent evt;
    Window win = make_selection_window(dpy);
    Atom target = utf8 ? XA_UTF8_STRING(dpy) : XA_STRING;
    /* FIXME: Should not use CurrentTime per ICCCM section 2.1 */
    XSetSelectionOwner(dpy, seltype, win, CurrentTime);
    while (1) {  /* wait for a SelectionRequest event */
      static uint clear = 0;
      static uint context = XCLIB_XCIN_NONE;
      static ulong sel_pos = 0;
      static Window cwin;
      static Atom pty;
      int finished;
      XNextEvent(dpy, &evt);
      finished = xcin(dpy, &cwin, evt, &pty, target, (uchar*)sel_buf, sel_len, &sel_pos, &context);
      if (evt.type == SelectionClear) { clear = 1; }
      if ((context == XCLIB_XCIN_NONE) && clear) { break; }
      if (finished) { break; }
    }
    XDestroyWindow(dpy,win);
  }
}


XCTRL_API uchar* get_selection(Display* dpy, char kind, Bool utf8)
{
  uchar *sel_buf=NULL;  /* buffer for selection data */
  ulong sel_len = 0;  /* length of sel_buf */
  XEvent evt;      /* X Event Structures */
  uchar*result=NULL;       /* null-terminated copy of sel_buf */
  uint context = XCLIB_XCOUT_NONE;
  Window win = make_selection_window(dpy);
  Atom seltype = selarg_to_seltype(dpy,kind);
  Atom target = utf8 ? XA_UTF8_STRING(dpy) : XA_STRING;
  if (seltype == XA_STRING) {
    sel_buf = (uchar*) XFetchBuffer(dpy, (int *) &sel_len, 0);
  } else {
    while (1) {
      if (context != XCLIB_XCOUT_NONE) { XNextEvent(dpy, &evt); }
      xcout(dpy, win, evt, seltype, target, &sel_buf, &sel_len, &context);
      if (context == XCLIB_XCOUT_FALLBACK) {
        context = XCLIB_XCOUT_NONE;
        target = XA_STRING;
        continue;
      }
      if (context == XCLIB_XCOUT_NONE) { break; }
    }
  }
  if (sel_buf) {
    result=(uchar*)calloc(sel_len+1,1);
    strncpy((char*)result, (char*)sel_buf, sel_len);
    result[sel_len]='\0';
    if (seltype == XA_STRING) { XFree(sel_buf); } else { free(sel_buf); }
  }
  XDestroyWindow(dpy,win);
  return result;
}
