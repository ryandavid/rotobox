# http://stackoverflow.com/questions/18136918/how-to-get-current-relative-directory-of-your-makefile
ROOT_DIR=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROTOBOX_3RD_PARTY_DIR=$(ROOT_DIR)/3rd_party
ROTOBOX_3RD_PARTY_BUILD_DIR=$(ROOT_DIR)/3rd_party_build
ROTOBOX_PKG_CONFIG_PATH=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig

########################################
# Rotobox Flags                        #
########################################
CFLAGS+=-O2 -g -Wall -Ifec
LDFLAGS="-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib"
LIBS=-lm -lpthread -ldl -lgeos_c -lrtlsdr -lusb-1.0 -lrasterlite2 -lspatialite -lsqlite3 -lproj -lgps -lmetar
CC=gcc
MAKE=make
CMAKE=cmake
ECHO=echo
UNAME=$(shell uname -s)
CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include -DMAKE_DUMP_978_LIB -DMAKE_DUMP_1090_LIB


ifeq ($(UNAME), Darwin)
LIB_EXT=dylib
else
LIB_EXT=so
endif
########################################
# 3rd-party lib locations              #
########################################
DUMP978_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/dump978
DUMP1090_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/dump1090

LIBRTLSDR_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/librtlsdr
LIBRTLSDR_BUILDDIR=$(LIBRTLSDR_SUBDIR)/build
LIBRTLSDR_MAKEFILE=$(LIBRTLSDR_BUILDDIR)/Makefile
LIBRTLSDR_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/librtlsdr.$(LIB_EXT)

LIBUSB_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libusb
LIBUSB_MAKEFILE=$(LIBUSB_SUBDIR)/Makefile
LIBUSB_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libusb-1.0.0.$(LIB_EXT)

GIFLIB_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/giflib
GIFLIB_MAKEFILE=$(GIFLIB_SUBDIR)/Makefile
GIFLIB_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libgif.$(LIB_EXT)

GPSD_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/gpsd
GPSD_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libgps.$(LIB_EXT)

LIBCAIRO_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libcairo
LIBCAIRO_MAKEFILE=$(LIBCAIRO_SUBDIR)/Makefile
LIBCAIRO_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libcairo.$(LIB_EXT)

PIXMAN_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/pixman
PIXMAN_MAKEFILE=$(PIXMAN_SUBDIR)/Makefile
PIXMAN_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/pixman.$(LIB_EXT)

LIBGEOS_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libgeos
LIBGEOS_MAKEFILE=$(LIBGEOS_SUBDIR)/Makefile
LIBGEOS_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libgeos.$(LIB_EXT)

LIBGEOTIFF_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libgeotiff
LIBGEOTIFF_MAKEFILE=$(LIBGEOTIFF_SUBDIR)/Makefile
LIBGEOTIFF_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libgeotiff.$(LIB_EXT)

LIBJPEG_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libjpeg
LIBJPEG_MAKEFILE=$(LIBJPEG_SUBDIR)/Makefile
LIBJPEG_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libjpeg.$(LIB_EXT)

LIBPNG_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libpng
LIBPNG_MAKEFILE=$(LIBPNG_SUBDIR)/Makefile
LIBPNG_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libpng.$(LIB_EXT)

LIBTIFF_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libtiff
LIBTIFF_MAKEFILE=$(LIBTIFF_SUBDIR)/Makefile
LIBTIFF_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libtiff.$(LIB_EXT)

LIBWEBP_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libwebp
LIBWEBP_MAKEFILE=$(LIBWEBP_SUBDIR)/Makefile
LIBWEBP_LIB=(ROTOBOX_3RD_PARTY_DIR)/lib/libwebp.$(LIB_EXT)

PROJ4_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/proj.4
PROJ4_MAKEFILE=$(PROJ4_SUBDIR)/Makefile
PROJ4_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libproj.$(LIB_EXT)

SQLITE_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/sqlite
SQLITE_MAKEFILE=$(SQLITE_SUBDIR)/Makefile
SQLITE3_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libsqlite3.$(LIB_EXT)

XZ_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/xz
XZ_MAKEFILE=$(XZ_SUBDIR)/Makefile
XZ_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/liblzma.$(LIB_EXT)

LIBMETAR_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/mdsplib
LIBMETAR_MAKEFILE=$(LIBMETAR_SUBDIR)/Makefile
LIBMETAR_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libmetar.a

LIBRASTERLITE2_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/librasterlite2
LIBRASTERLITE2_MAKEFILE=$(LIBRASTERLITE2_SUBDIR)/Makefile
LIBRASTERLITE2_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/librasterlite2.$(LIB_EXT)

LIBSPATIALITE_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libspatialite
LIBSPATIALITE_MAKEFILE=$(LIBSPATIALITE_SUBDIR)/Makefile
LIBSPATIALITE_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libspatialite.$(LIB_EXT)

CURL_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/curl
CURL_MAKEFILE=$(CURL_SUBDIR)/Makefile
CURL_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libcurl.$(LIB_EXT)

LIBXML2_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libxml2
LIBXML2_MAKEFILE=$(LIBXML2_SUBDIR)/Makefile
LIBXML2_LIB=$(ROTOBOX_3RD_PARTY_DIR)/lib/libxml2.$(LIB_EXT)


########################################
# TODO: Convert these to libs          #
########################################
DUMP978_DEPENDS=$(ROTOBOX_3RD_PARTY_DIR)/dump978/dump978.o $(ROTOBOX_3RD_PARTY_DIR)/dump978/uat_decode.o \
				$(ROTOBOX_3RD_PARTY_DIR)/dump978/fec.o $(ROTOBOX_3RD_PARTY_DIR)/dump978/fec/init_rs_char.o \
				$(ROTOBOX_3RD_PARTY_DIR)/dump978/fec/decode_rs_char.o

DUMP1090_DEPENDS=$(ROTOBOX_3RD_PARTY_DIR)/dump1090/dump1090.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/convert.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/anet.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/cpr.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/demod_2400.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/icao_filter.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/interactive.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/mode_ac.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/mode_s.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/net_io.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/stats.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/track.o \
				 $(ROTOBOX_3RD_PARTY_DIR)/dump1090/util.o $(ROTOBOX_3RD_PARTY_DIR)/dump1090/crc.o

MONGOOSE_DEPENDS=$(ROTOBOX_3RD_PARTY_DIR)/mongoose/mongoose.o

ifeq ($(UNAME), Darwin)
# TODO: Putting GCC in C11 mode breaks things.
CFLAGS+=-std=c11 -DMISSING_GETTIME -DMISSING_NANOSLEEP
DUMP1090_DEPENDS+=$(ROTOBOX_3RD_PARTY_DIR)/dump1090/compat/clock_gettime/clock_gettime.o \
				  $(ROTOBOX_3RD_PARTY_DIR)/dump1090/compat/clock_nanosleep/clock_nanosleep.o
endif

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

rotobox: rotobox.o gdl90.o database.o database_maintenance.o api.o $(DUMP978_DEPENDS) \
		 $(DUMP1090_DEPENDS) $(MONGOOSE_DEPENDS)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

rotobox-deps: libgeos librtlsdr libusb librasterlite2 libspatialite sqlite proj4 gpsd libmetar

clean:
	rm -rf *.o

########################################
# dump978                              #
########################################
dump978:
	$(ECHO) "Building dump978"
	$(MAKE) -C $(DUMP978_SUBDIR)

########################################
# dump1090                             #
########################################
dump1090:
	$(ECHO) "Building dump1090"
	$(MAKE) -C $(DUMP1090_SUBDIR)

########################################
# libusb                               #
########################################
$(LIBUSB_LIB): libusb

libusb: $(LIBUSB_MAKEFILE)
	$(MAKE) -C $(LIBUSB_SUBDIR) install

$(LIBUSB_MAKEFILE):
	cd $(LIBUSB_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./bootstrap.sh && sleep 2 && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# Librtlsdr                            #
########################################
$(LIBRTLSDR_LIB): librtlsdr

librtlsdr: $(LIBUSB_LIB) $(LIBRTLSDR_MAKEFILE)
	$(ECHO) "Building librtlsdr"
	$(MAKE) -C $(LIBRTLSDR_BUILDDIR) install

$(LIBRTLSDR_MAKEFILE):
	$(ECHO) "Need to run cmake for librtlsdr"
	mkdir -p $(LIBRTLSDR_BUILDDIR) && rm -f $(LIBRTLSDR_BUILDDIR)/CMakeCache.txt
	cd $(LIBRTLSDR_BUILDDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	cmake ../ -DCMAKE_INSTALL_PREFIX=$(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# GIFLIB                               #
########################################
$(GIFLIB_LIB): giflib

giflib: $(GIFLIB_MAKEFILE)
	$(MAKE) -C $(GIFLIB_SUBDIR) install

$(GIFLIB_MAKEFILE):
	cd $(GIFLIB_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# gpsd                                 #
########################################
$(GPSD_LIB): gpsd

gpsd:
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
	cd $(GPSD_SUBDIR) && \
	scons prefix=$(ROTOBOX_3RD_PARTY_BUILD_DIR) python_libdir=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/python && \
	scons install

	# HACKY HACKY HACKY. Need to fix this.
	install_name_tool -id $(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/libgps.dylib $(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/libgps.dylib

########################################
# libcairo                             #
########################################
$(LIBCAIRO_LIB): libcairo

libcairo: $(PIXMAN_LIB) $(LIBCAIRO_MAKEFILE)
	$(MAKE) -C $(LIBCAIRO_SUBDIR) install

$(LIBCAIRO_MAKEFILE):
	cd $(LIBCAIRO_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# pixman                               #
########################################
$(PIXMAN_LIB): pixman

pixman: $(PIXMAN_MAKEFILE)
	$(MAKE) -C $(PIXMAN_SUBDIR) install

$(PIXMAN_MAKEFILE):
	cd $(PIXMAN_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libgeos                              #
########################################
$(LIBGEOS_LIB): libgeos

libgeos: $(LIBGEOS_MAKEFILE)
	$(MAKE) -C $(LIBGEOS_SUBDIR) install

$(LIBGEOS_MAKEFILE):
	cd $(LIBGEOS_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libgeotiff                           #
########################################
$(LIBGEOTIFF_LIB): libgeotiff

libgeotiff: libtiff $(LIBGEOTIFF_MAKEFILE)
	$(MAKE) -C $(LIBGEOTIFF_SUBDIR) install

$(LIBGEOTIFF_MAKEFILE):
	cd $(LIBGEOTIFF_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libjpeg                              #
########################################
$(LIBJPEG_LIB): libjpeg

libjpeg: $(LIBJPEG_MAKEFILE)
	$(MAKE) -C $(LIBJPEG_SUBDIR) install

$(LIBJPEG_MAKEFILE):
	cd $(LIBJPEG_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)
	# Hacky, but libjpeg tries to install its man pages in an odd location.
	# Make the directory here.
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/man/man1

########################################
# libpng                               #
########################################
$(LIBPNG_LIB): libpng

libpng: $(LIBPNG_MAKEFILE)
	$(MAKE) -C $(LIBPNG_SUBDIR) install

$(LIBPNG_MAKEFILE):
	cd $(LIBPNG_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libtiff                              #
########################################
$(LIBTIFF_LIB): libtiff

libtiff: $(LIBTIFF_MAKEFILE)
	$(MAKE) -C $(LIBTIFF_SUBDIR) install

$(LIBTIFF_MAKEFILE):
	cd $(LIBTIFF_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libwebp                              #
########################################
$(LIBWEBP_LIB): libwebp

libwebp: $(LIBWEBP_MAKEFILE)
	$(MAKE) -C $(LIBWEBP_SUBDIR) install

$(LIBWEBP_MAKEFILE):
	cd $(LIBWEBP_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh && sleep 2 && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# proj.4                               #
########################################
$(PROJ4_LIB): proj4

proj4: $(PROJ4_MAKEFILE)
	$(MAKE) -C $(PROJ4_SUBDIR) install

$(PROJ4_MAKEFILE):
	cd $(PROJ4_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# sqlite                               #
########################################
$(SQLITE3_LIB): sqlite

sqlite: $(SQLITE_MAKEFILE)
	$(MAKE) -C $(SQLITE_SUBDIR) install

$(SQLITE_MAKEFILE):
	cd $(SQLITE_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --disable-tcl

########################################
# xz                                   #
########################################
$(XZ_LIB): xz

xz: $(XZ_MAKEFILE)
	$(MAKE) -C $(XZ_SUBDIR) install

$(XZ_MAKEFILE):
	cd $(XZ_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libmetar                             #
########################################
$(LIBMETAR_LIB): libmetar

libmetar:
	$(MAKE) -C $(LIBMETAR_SUBDIR) install

########################################
# librasterlite2                       #
########################################
$(LIBRASTERLITE2_LIB): librasterlite2

librasterlite2: $(CURL_LIB) $(LIBXML2_LIB) $(LIBSPATIALITE_LIB) $(PROJ4_LIB) $(LIBPNG_LIB) \
				$($LIBWEBP_LIB) $(LIBJPEG_LIB) $(LIBGEOTIFF_LIB) $(XZ_LIB) $(LIBCAIRO_LIB) $(LIBRASTERLITE2_MAKEFILE)
	$(MAKE) -C $(LIBRASTERLITE2_SUBDIR) install

$(LIBRASTERLITE2_MAKEFILE):
	cd $(LIBRASTERLITE2_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# spatialite                           #
########################################
$(LIBSPATIALITE_LIB): spatialite

spatialite: $(SQLITE3_LIB) $(PROJ4_LIB) $(LIBGEOS_LIB) $(LIBSPATIALITE_MAKEFILE)
	$(MAKE) -C $(LIBSPATIALITE_SUBDIR) install

$(LIBSPATIALITE_MAKEFILE):
	cd $(LIBSPATIALITE_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --disable-freexl --disable-libxml2 --with-geosconfig=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/bin/geos-config

########################################
# curl                                 #
########################################
$(CURL_LIB): curl

curl: $(CURL_MAKEFILE)
	$(MAKE) -C $(CURL_SUBDIR) install

$(CURL_MAKEFILE):
	cd $(CURL_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./buildconf && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libxml2                              #
########################################
$(LIBXML2_LIB): libxml2

libxml2: $(LIBXML2_MAKEFILE)
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
	$(MAKE) -C $(LIBXML2_SUBDIR) install

$(LIBXML2_MAKEFILE):
	cd $(LIBXML2_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib/pkgconfig \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib \
	./autogen.sh --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --with-python-install-dir=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
