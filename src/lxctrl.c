/*

Lua bindings to the wmctrl window manager control library.

(C) 2010 by Jeffrey Pohlmeyer <yetanothergeek@gmail.com>


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
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <X11/Xlib.h>

#define XCTRL_API static
#include "xctrl.c"
#undef XCTRL_API



#define XCTRL_META_NAME "xctrl"

#define SetTableValue(name,value,pusher) \
  lua_pushstring(L, name); \
  pusher(L, value); \
  lua_rawset(L,-3);

#define SetTableStr(name,value) SetTableValue(name,value,lua_pushstring)
#define SetTableBool(name,value) SetTableValue(name,value,lua_pushboolean)
#define SetTableNum(name,value) SetTableValue(name,(lua_Number)value,lua_pushnumber)

#define ERR_BUF_SIZE 128

static char lwmc_error_buffer[ERR_BUF_SIZE];

static int lwmc_handle_error(Display *dpy, XErrorEvent*ev)
{
  memset(lwmc_error_buffer, '\0', ERR_BUF_SIZE);
  if ( !ev ) {
    strncpy(lwmc_error_buffer, "NULL event\n", ERR_BUF_SIZE-1);
  } else if ( !dpy ) {
    strncpy(lwmc_error_buffer, "NULL display\n", ERR_BUF_SIZE-1);
  } else {
    XGetErrorText(dpy, ev->error_code, lwmc_error_buffer, ERR_BUF_SIZE-1);
  }
  return -1;
}



typedef struct _XCtrl {
  Display* dpy;
  char *dpyname;
  char* charset;
  XErrorHandler old_err_handler;
} XCtrl;

static XCtrl*wm=NULL;


static XCtrl*lwmc_check_obj(lua_State*L) {
  return (XCtrl*)luaL_checkudata(L,1,XCTRL_META_NAME);
}



static int lwmc_failure(lua_State *L, const char*msg)
{
  lua_pushnil(L);
  lua_pushstring(L,msg);
  return 2;
}



static int lwmc_new(lua_State *L)
{
  if (wm==NULL) { 
    const char*req_dpyname=luaL_optstring(L,1,NULL);
    char*act_dpyname=XDisplayName(req_dpyname);
    Display*dpy=XOpenDisplay(req_dpyname);
    if (!dpy) {
      return lwmc_failure(L, "Can't open display.");
    }
    wm=(XCtrl*)lua_newuserdata(L,sizeof(XCtrl));
    memset(wm,0,sizeof(XCtrl));
    if (act_dpyname) { wm->dpyname=strdup(act_dpyname); }
    wm->dpy=dpy;
    luaL_getmetatable(L, XCTRL_META_NAME);
    lua_setmetatable(L, -2);
    wm->old_err_handler=XSetErrorHandler(lwmc_handle_error);
    if (lua_gettop(L)>1) {
      int force_utf8=lua_toboolean(L,2);
      const char*charset=luaL_optstring(L,3,NULL);
      if (charset) { wm->charset=strdup(charset); }
      init_charset(force_utf8,wm->charset);
    }
  } else {
    luaL_error(L,"Only one instance of "XCTRL_META_NAME" is allowed.");
    return 0;
  }
  return 1;
}



static int lwmc_gc(lua_State *L)
{  
  XCtrl*ud=lwmc_check_obj(L);
  XSetErrorHandler(wm->old_err_handler);
  XCloseDisplay(ud->dpy);
  if (wm->dpyname) { free(ud->dpyname); }
  if (wm->charset) { free(ud->charset); }
  wm=NULL;
  return 1;
}



static int lwmc_get_wm_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,supporting_wm_check(ud->dpy));
  return 1;
}



static int lwmc_get_wm_name(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  char*tmp=get_wm_name(ud->dpy);
  lua_pushstring(L,tmp);
  if (tmp) { free(tmp); }
  return 1;
}



static int lwmc_get_wm_class(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  char*tmp=get_wm_class(ud->dpy);
  lua_pushstring(L,tmp);
  if (tmp) { free(tmp); }
  return 1;
}



static int lwmc_get_wm_pid(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,get_wm_pid(ud->dpy));
  return 1;

}



static int lwmc_get_showing_desktop(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,get_showing_desktop(ud->dpy));
  return 1;
}



static int lwmc_get_display(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushstring(L,ud->dpyname);
  return 1;
}



static Window check_window(lua_State *L, XCtrl*ud, int argnum)
{
  Window win;
  memset(lwmc_error_buffer, '\0', ERR_BUF_SIZE);
  luaL_argcheck(L,lua_isnumber(L,argnum), argnum, "expected window id");
  win=lua_tonumber(L,argnum);
  return win;
}



static Bool lwmc_success(lua_State *L, XCtrl*ud)
{
  XSync(ud->dpy,False);
  if (lwmc_error_buffer[0]!=0) { 
    lua_pushnil(L); \
    lua_pushstring(L,lwmc_error_buffer);
    return False;
  }
  return True;
}



static int lwmc_get_win_class(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  char*s=get_window_class(ud->dpy, win);
  if (lwmc_success(L,ud)) {
    lua_pushstring(L,s?s:"");
    if (s) { free(s); }
    return 1;
  } else {
    if (s) { free(s); }
    return 2;
  }
}



static int lwmc_get_win_title(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  char*s=get_window_title(ud->dpy, win);
  if (lwmc_success(L,ud)) {
    lua_pushstring(L,s?s:"");
    if (s) { free(s); }
    return 1;
  } else {
    if (s) { free(s); }
    return 2;
  }
}



static int lwmc_set_win_title(lua_State *L)
{ 
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  const char*name=luaL_checkstring(L,3);
  const char*mode=luaL_optstring(L,4,"T");
  luaL_argcheck(L, (strlen(mode)==1) && (strchr("NTI", mode[0])), 4, "mode must be 'T' 'I' or 'N'");
  set_window_title(ud->dpy, win, name, mode[0]);
    if (lwmc_success(L,ud)) {
    lua_pushboolean(L,True);
    return 1;
  } else { return 2; }
}



static int lwmc_set_win_geom(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  long x=luaL_checknumber(L,3); 
  long y=luaL_checknumber(L,4);
  long w=luaL_checknumber(L,5);
  long h=luaL_checknumber(L,6);
  long g=luaL_optnumber(L,7,0); 
  if (set_window_geom(ud->dpy, win, g,x,y,w,h)) {
    return lwmc_failure(L,"move/resize failed");
  } else if (lwmc_success(L,ud)) {
    lua_pushboolean(L,True);
    return 1;
  } else {
    return 2;
  }
}



static int lwmc_close_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  if (close_window(ud->dpy, win)) {
    return lwmc_failure(L,"failed to close window");
  } else if (lwmc_success(L,ud)) {
    lua_pushboolean(L,True);
    return 1;
  } else {
    return 2;
  }
}



static int lwmc_activate_win(lua_State *L)
{  
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  int switch_desktop=True;
  if (lua_gettop(L)>2) {
    switch_desktop=lua_toboolean(L,3);
  }
  activate_window(ud->dpy, win, switch_desktop);
  if (lwmc_success(L,ud)) {
    lua_pushboolean(L,True);
    return 1;
  } else {
    return 2;
  }
}



static int lwmc_iconify_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  Bool rv=iconify_window(ud->dpy, win);
  if (lwmc_success(L,ud)) {
    if (rv) {
      lua_pushboolean(L,True);
      return 1;
    } else {
      return  lwmc_failure(L,"failed to iconify window");
    }
  } else {
    return 2;
  }
}



static int lwmc_set_desk_of_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  int desk=luaL_checknumber(L,3);
  int ok=send_window_to_desktop(ud->dpy,win,desk-1);
  if (lwmc_success(L,ud)) {
    if (ok) { 
      lua_pushboolean(L,True);
      return 1;
    } else {
      return lwmc_failure(L,"sendto failed");
    }
  } else { return 2; }
}



static int lwmc_get_desk_of_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  long desk=get_desktop_of_window(ud->dpy,win);
  if (lwmc_success(L,ud)) {
    if ( desk > -2 ) {
      lua_pushnumber(L,desk==-1?-1:desk+1);
      return 1;
    } else {
      return lwmc_failure(L,"can't find desktop");
    }
  } else { return 2; }
}



static int lwmc_get_pid_of_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  long pid=get_win_pid(ud->dpy,win);
  if (lwmc_success(L,ud)) {
    if ( pid > 0 ) {
      lua_pushnumber(L,pid);
      return 1;
    } else {
      return lwmc_failure(L,"unsupported feature");
    }
  } else { return 2; }
}



static int lwmc_get_win_client(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  char*cm=get_client_machine(ud->dpy,win);
  if (lwmc_success(L,ud)) {
    if ( cm ) {
      lua_pushstring(L,cm);
      free(cm);
      return 1;
    } else {
      return lwmc_failure(L,"unknown client");
    }
  } else {
    if (cm) { free(cm); }
    return 2;
  }
}



static int lwmc_set_win_state(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  ulong action;
  const char *p1; 
  const char *p2;
  const char* actions[]={"add", "remove", "toggle", NULL};
  switch (luaL_checkoption(L,3,NULL,actions)) {
    case 0: action= _NET_WM_STATE_ADD; break;
    case 1: action= _NET_WM_STATE_REMOVE; break;
    default: action= _NET_WM_STATE_TOGGLE; break;
  }
  p1=luaL_checkstring(L,4);
  luaL_argcheck(L,p1&&*p1, 4, "property can't be empty");
  p2=luaL_optstring(L,5,NULL);
  luaL_argcheck(L,(!p2)||*p2, 5, "property can't be empty");
  set_window_state(ud->dpy,win,action, p1, p2);
  if (lwmc_success(L,ud)) {
    lua_pushboolean(L,True);
    return 1;
  } else {
    return 2;
  }
}



static int lwmc_pick_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  int btn=-1;
  if (lua_gettop(L)>1) {
    btn=luaL_checknumber(L,2);
    luaL_argcheck(L,(btn>=1)&&(btn<=3),2,"Button must be between 1 and 3");
  }

  lua_pushnumber(L,select_window(ud->dpy, btn));
  return 1;
}



static int lwmc_get_active_win(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,get_active_window(ud->dpy));
  return 1;
}



static int lwmc_root_win(lua_State *L)
{ 
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,DefaultRootWindow(ud->dpy));
  return 1;
}



static int lwmc_get_win_geom(lua_State *L) {
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  Geometry inf={0,0,0,0};
  get_window_geom(ud->dpy, win, &inf);
  if (lwmc_success(L,ud)) {
    lua_newtable(L);
    SetTableNum("x", inf.x);
    SetTableNum("y", inf.y);
    SetTableNum("w", inf.w);
    SetTableNum("h", inf.h);
    return 1;
  } else {
    return 2;
  }
}



static int lwmc_get_workarea(lua_State *L) {
  XCtrl*ud=lwmc_check_obj(L);
  int desknum=luaL_checknumber(L,2);
  Geometry inf={0,0,0,0};
  if (get_workarea_geom(ud->dpy, &inf, desknum-1)) {
    lua_newtable(L);
    SetTableNum("x", inf.x);
    SetTableNum("y", inf.y);
    SetTableNum("w", inf.w);
    SetTableNum("h", inf.h);
  } else {
    lua_pushnil(L);
  }
  return 1;
}



static int lwmc_get_desk_geom(lua_State *L) {
  XCtrl*ud=lwmc_check_obj(L);
  int desknum=luaL_checknumber(L,2);
  Geometry inf={0,0,0,0};
  if (get_desktop_geom(ud->dpy, desknum-1, &inf)) {
    lua_newtable(L);
    SetTableNum("x", inf.x);
    SetTableNum("y", inf.y);
    SetTableNum("w", inf.w);
    SetTableNum("h", inf.h);
  } else {
    lua_pushnil(L);
  }
  return 1;
}



static int lwmc_get_win_list(lua_State* L)
{
  XCtrl*ud=lwmc_check_obj(L);
  unsigned long size;
  Window*list=get_window_list(ud->dpy, &size);
  if (list) { 
    unsigned long i;
    size= size / sizeof(Window);
    lua_newtable(L);
    for (i=0; i<size; i++) {
      lua_pushnumber(L,i+1);
      lua_pushnumber(L,list[i]);
      lua_rawset(L,-3);
    }
    free(list);
    return 1;
  } else {
    return lwmc_failure(L,"Failed to retreive client list.");
  }
}



static int lwmc_tostring(lua_State *L)
{
  lua_pushfstring(L,"%s (%p)", XCTRL_META_NAME, wm);
  return 1;
}



typedef struct _DtCbData {
  lua_State *L;
  unsigned char count;
} DtCbData;



static int lwmc_get_desk_name(lua_State *L)
{ 
  XCtrl*ud=lwmc_check_obj(L);
  int desknum=luaL_checknumber(L,2);
  int force_utf8 = (lua_gettop(L)>2) ? lua_toboolean(L,3) : 0;
  char*name=get_desktop_name(ud->dpy, desknum-1, force_utf8);
  lua_pushstring(L, name);
  if (name) { free(name); }
  return 1;
}



static int lwmc_set_showing_desktop(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  luaL_argcheck(L, lua_gettop(L)>1, 2, "expected boolean"); 
  set_showing_desktop(ud->dpy, lua_toboolean(L,2));
  return 0;
}



static int lwmc_set_num_desks(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  int n=luaL_checknumber(L,2);
  if (set_number_of_desktops(ud->dpy,n)) {
    lua_pushboolean(L,True);
    return 1;
  } else {
    return lwmc_failure(L,"set #desktops failed");
  }
}



static int lwmc_set_curr_desk(lua_State *L)
{ 
  XCtrl*ud=lwmc_check_obj(L);
  int n=luaL_checknumber(L,2);
  lua_pushboolean(L,set_current_desktop(ud->dpy,n-1));
  return 1;
}



static int lwmc_set_desk_vport(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  int x=luaL_checknumber(L,2);
  int y=luaL_checknumber(L,3);
  lua_pushboolean(L,change_viewport(ud->dpy,x,y));
  return 1;
}



static int lwmc_set_desk_geom(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  int x=luaL_checknumber(L,2);
  int y=luaL_checknumber(L,3);
  lua_pushboolean(L,change_geometry(ud->dpy,x,y));
  return 1;
}



static int lwmc_get_num_desks(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,get_number_of_desktops(ud->dpy));
  return 1;
}



static int lwmc_get_curr_desk(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  lua_pushnumber(L,get_current_desktop(ud->dpy)+1);
  return 1;
}



static int lwmc_send_keys(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  Window win=check_window(L,ud,2);
  const char*keys=luaL_checkstring(L,3);
  send_keystrokes(ud->dpy, win, keys);
  return 0;
}



static int lwmc_convert_locale(lua_State *L)
{
  const char*src,*from_charset,*to_charset;
  char*dst;
  lwmc_check_obj(L);
  src=luaL_checkstring(L,2);
  from_charset=luaL_checkstring(L,3);
  to_charset=luaL_checkstring(L,4);
  dst=convert_locale(src,from_charset,to_charset);
  if (dst) {
    lua_pushstring(L,dst);
    free(dst);
    return 1;
  } else {
    return 0;
  }
}



static int lwmc_do_events(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  int n=luaL_optnumber(L,2,1);
  int i;
  for (i=0; i<n; i++) {
    usleep(100000);
    XSync(ud->dpy,False);
  }
  return 0;
}


static int lwmc_get_selection(lua_State *L)
{
  XCtrl*ud=lwmc_check_obj(L);
  const char*kind=luaL_optstring(L,2,"p");
  Bool utf8=lua_gettop(L)>2?lua_toboolean(L,3):False;
  uchar*sel=get_selection(ud->dpy, kind[0], utf8);
  if (sel) {
    lua_pushstring(L, (char*)sel);
    free(sel);
    return 1;
  }
  return 0;
}



static int lwmc_set_selection(lua_State *L)
{ 
  XCtrl*ud=lwmc_check_obj(L);
  const char*sel=luaL_checkstring(L,2);
  const char*kind=luaL_optstring(L,3,"p");
  Bool utf8=lua_gettop(L)>3?lua_toboolean(L,3):False;
  set_selection(ud->dpy, kind[0], (char*)sel, utf8);
  return 0;
}



static const struct luaL_Reg lwmc_funcs[] = {
  {"new",             lwmc_new},
  {"get_win_list",    lwmc_get_win_list},
  {"get_win_title",   lwmc_get_win_title},
  {"set_win_title",   lwmc_set_win_title},
  {"get_win_class",   lwmc_get_win_class},
  {"activate_win",    lwmc_activate_win},
  {"iconify_win",     lwmc_iconify_win},
  {"set_win_state",   lwmc_set_win_state},
  {"set_win_geom",    lwmc_set_win_geom},
  {"get_win_geom",    lwmc_get_win_geom},
  {"get_active_win",  lwmc_get_active_win}, 
  {"pick_win",        lwmc_pick_win},  
  {"root_win",        lwmc_root_win},
  {"set_desk_of_win", lwmc_set_desk_of_win},
  {"get_desk_of_win", lwmc_get_desk_of_win},
  {"get_pid_of_win",  lwmc_get_pid_of_win},
  {"close_win",       lwmc_close_win},
  {"get_win_client",  lwmc_get_win_client},
  {"get_num_desks",   lwmc_get_num_desks},
  {"set_num_desks",   lwmc_set_num_desks},
  {"get_curr_desk",   lwmc_get_curr_desk},
  {"set_curr_desk",   lwmc_set_curr_desk},
  {"get_display",     lwmc_get_display},
  {"get_wm_win",      lwmc_get_wm_win},
  {"get_wm_name",     lwmc_get_wm_name},
  {"get_wm_class",    lwmc_get_wm_class},
  {"get_wm_pid",      lwmc_get_wm_pid},
  {"get_desk_name",   lwmc_get_desk_name},
  {"get_workarea",    lwmc_get_workarea},
  {"get_desk_geom",   lwmc_get_desk_geom},
  {"set_desk_vport",  lwmc_set_desk_vport},
  {"set_desk_geom",   lwmc_set_desk_geom},
  {"set_showing_desk",lwmc_set_showing_desktop},
  {"get_showing_desk",lwmc_get_showing_desktop},
  {"send_keys",       lwmc_send_keys},
  {"do_events",       lwmc_do_events},
  {"convert_locale",  lwmc_convert_locale},
  {"get_selection",   lwmc_get_selection},
  {"set_selection",   lwmc_set_selection},
  {NULL,NULL}
};


int luaopen_xctrl(lua_State *L);

int luaopen_xctrl(lua_State *L)
{
  luaL_newmetatable(L, XCTRL_META_NAME);
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);

  lua_pushstring(L,"__gc");
  lua_pushcfunction(L,lwmc_gc);
  lua_rawset(L,-3);

  lua_pushstring(L,"__tostring");
  lua_pushcfunction(L,lwmc_tostring);
  lua_rawset(L,-3);

#if LUA_VERSION_NUM < 502
  luaL_register(L, NULL, &lwmc_funcs[1]);
  luaL_register(L, XCTRL_META_NAME, lwmc_funcs);
#else
  luaL_setfuncs(L,lwmc_funcs,0);
#endif
  return 1;
}

