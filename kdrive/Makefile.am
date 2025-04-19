SERVER_SUBDIRS = kdrive

SUBDIRS = src   \
	  linux \
	  fbdev

DIST_SUBDIRS = fbdev src linux

relink:
	$(AM_V_at)for i in $(SERVER_SUBDIRS) ; do $(MAKE) -C $$i relink || exit 1 ; done
