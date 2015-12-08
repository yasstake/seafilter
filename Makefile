#/* Copyright 2011 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
# *
# * This file is part of smfilter.
# *
# * Smfilter is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, version 3 of the License.
# *
# * Smfilter is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with smfilter. If not, see <http://www.gnu.org/licenses/>.
# */

CC	= gcc
CFLAGS	= -g -Wall -DHAS_STRPTIME -DEXT_RADIUS_TAG
LDFLAGS	= -lm
VER = smfilter-r$(shell svnversion | tr -d M)

all: smfilter

smfilter: smfilter.o bstring.o osm_func.o libhpxml.o sector_calc.o smlog.o
	gcc -o smfilter smfilter.o bstring.o osm_func.o libhpxml.o sector_calc.o smlog.o -lm

smfilter.o: smfilter.c smlog.h bstring.h

osm_func.o: osm_func.c osm_inplace.h

bstring.o: bstring.c bstring.h

libhpxml.o: libhpxml.c libhpxml.h bstring.h

sector_calc.o: sector_calc.c seamark.h

smlog.o: smlog.c smlog.h

clean:
	rm -f *.o smfilter

dist: smfilter
	if test -e $(VER) ; then \
		rm -r $(VER) ; \
	fi
	mkdir $(VER) $(VER)/man
	cp *.c *.h smfilter Makefile testlight.osm $(VER)
	cp man/smfilter.1 $(VER)/man
	tar cvfj $(VER).tbz2 $(VER)

install:
	cp smfilter /usr/local/bin/

.PHONY: clean dist install

