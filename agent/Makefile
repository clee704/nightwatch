package = nitch_agent
version = 0.0-alpha
tarname = $(package)
distdir = $(tarname)-$(version)

export prefix=/usr/local
export exec_prefix=$(prefix)
export bindir=$(exec_prefix)/bin

all clean check install uninstall nitch_agent:
	$(MAKE) -C src $@

dist: $(distdir).tar.gz

$(distdir).tar.gz: FORCE $(distdir)
	tar chof - $(distdir) |\
	  gzip -9 -c >$@
	rm -rf $(distdir)

FORCE:
	-rm $(distdir).tar.gz &> /dev/null
	-rm -rf $(distdir) &> /dev/null

$(distdir):
	mkdir -p $@/src
	cp -alt $@ Makefile
	cp -alt $@/src src/Makefile src/main.c

distcheck: $(distdir).tar.gz
	gzip -cd $+ | tar xvf -
	$(MAKE) -C $(distdir) all check
	$(MAKE) -C $(distdir) DESTDIR=./$(distdir)/_inst \
	  install uninstall
	$(MAKE) -C $(distdir) clean
	rm -rf $(distdir)
	@echo "*** Package $(distdir).tar.gz ready for distribution."

.PHONY: FORCE all clean check dist distcheck
.PHONY: install uninstall
