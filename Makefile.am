.PHONY: gentags

SUBDIRS = src

#install menu descriptions in $(datadir)
dist_pkgdata_DATA = menus.json

gentags:
	geany -g -P geany.c.tags src/*.[ch]
