The X11 control library is a shared library and a [Lua](http://www.lua.org/) binding that allows you to query and manipulate various aspects of an X11 window manager and the windows it manages.

It is based largely on Tomas Styblo's [wmctrl](http://tomas.styblo.name/wmctrl/) command line tool, but has been transformed into a C library with some additional features and a Lua binding.

What it can do:

  * Return a list of all top-level windows.
  * Retrieve window information such as title, class name, and geometry.
  * Set the title of a window.
  * Move and resize windows.
  * Move windows from one virtual desktop to another.
  * Set various window properties, e.g. "minimized", "sticky", "topmost".
  * Raise a window and give it the focus.
  * Politely request the closing of a window.
  * Send "fake" keyboard events to a window.
  * Set and get the X selections and clipboard.

It is intended to work with any EWMH / NetWM compatible window manager, but since support for these standards varies between different window managers and clients, some functionality might be missing in some environments.