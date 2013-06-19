DLL_EXT=.so
LIBNAME=libxctrl${DLL_EXT}
MODNAME=xctrl${DLL_EXT}

LUA=lua

INSTALL=install
DESTDIR=`${LUA} -e 'p=package.cpath:gsub(".-(/[^;]+/).*","%1");print(p)'`

WARN_FLAGS=-Wall -pedantic -Wshadow -Wunused -Wbad-function-cast -Wmissing-prototypes
VERSION=1.09

CFLAGS= ${EXTRA_CFLAGS} -Wall -DVERSION=\"$(VERSION)\"
LDFLAGS=${EXTRA_LDFLAGS} -lX11 -lXmu

ifeq ($(DEBUG), 1)
 LDFLAGS += -ggdb3
 CFLAGS += -O0 -ggdb3
else
  ifeq ($(SYMS), 1)
    CFLAGS += -Os -fno-inline
    LDFLAGS += 
  else
    CFLAGS += -Os
    LDFLAGS += -s
  endif
endif
CFLAGS += ${WARN_FLAGS}


lua: $(MODNAME)_clean $(MODNAME)

lib: $(LIBNAME)_clean $(LIBNAME)

$(LIBNAME): xctrl.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared $^ -o $@


$(MODNAME): lxctrl.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared $^ -o $@


%.o: %.c
	$(CC) $(CFLAGS) -c $<


$(LIBNAME)_clean:
	rm -f *.o $(LIBNAME)

$(MODNAME)_clean:
	rm -f *.o $(MODNAME)

install: $(MODNAME)
	$(INSTALL) -d -m 755	${DESTDIR}
	$(INSTALL) -m 755	$^ ${DESTDIR}
	@echo
	@echo Installed \"$^\" in ${DESTDIR} "(OK)"
	@echo


clean:
	$(RM) *.o $(LIBNAME) $(MODNAME)

