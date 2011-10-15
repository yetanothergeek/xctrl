PKGNAME=xctrl

default:
	@$(MAKE) --no-print-directory -C src

%:
	@$(MAKE) --no-print-directory -C src $@


clean:
	$(RM) $(PKGNAME)-*.tar.gz
	$(MAKE) -C src clean


dist: clean
	date +$(PKGNAME)-%Y-%m-%d > PKG_NAME
	rm -rf `cat PKG_NAME`
	mkdir -p `cat PKG_NAME`
	cp -rp * `cat PKG_NAME` 2>/dev/null || true 
	rm -f `cat PKG_NAME`/PKG_NAME
	rm -rf `cat PKG_NAME`/`cat PKG_NAME`
	tar --owner=0 --group=0 -zcf `cat PKG_NAME`.tar.gz `cat PKG_NAME`
	rm -rf `cat PKG_NAME`
	rm -f PKG_NAME

