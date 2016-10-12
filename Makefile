# http://stackoverflow.com/questions/18136918/how-to-get-current-relative-directory-of-your-makefile
ROOT_DIR=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROTOBOX_3RD_PARTY_DIR=$(ROOT_DIR)/3rd_party
ROTOBOX_3RD_PARTY_BUILD_DIR=$(ROOT_DIR)/3rd_party_build
ROTOBOX_3RD_PARTY_LIB=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/lib
ROTOBOX_3RD_PARTY_INCLUDE=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/include
ROTOBOX_PKG_CONFIG_PATH=$(ROTOBOX_3RD_PARTY_LIB)/pkgconfig

########################################
# Rotobox Flags                        #
########################################
CFLAGS+=-O2 -g -Wall -Ifec
LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB)
LIBS=-lm -lpthread -ldl -lgeos_c -lusb-1.0 -lspatialite -lrasterlite2 -lsqlite3 \
	 -lrtlsdr -lproj -lgps -lmetar
CC=gcc
MAKE=make
CMAKE=cmake
ECHO=echo
UNAME=$(shell uname -s)
CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) -DMAKE_DUMP_978_LIB -DMAKE_DUMP_1090_LIB

########################################
# 3rd-party lib locations              #
########################################
DUMP978_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/dump978
DUMP1090_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/dump1090

LIBRTLSDR_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/librtlsdr
LIBRTLSDR_BUILDDIR=$(LIBRTLSDR_SUBDIR)/build
LIBRTLSDR_MAKEFILE=$(LIBRTLSDR_BUILDDIR)/Makefile

LIBUSB_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libusb
LIBUSB_MAKEFILE=$(LIBUSB_SUBDIR)/Makefile

GIFLIB_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/giflib
GIFLIB_MAKEFILE=$(GIFLIB_SUBDIR)/Makefile

GPSD_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/gpsd

LIBCAIRO_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libcairo
LIBCAIRO_MAKEFILE=$(LIBCAIRO_SUBDIR)/Makefile

PIXMAN_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/pixman
PIXMAN_MAKEFILE=$(PIXMAN_SUBDIR)/Makefile

LIBGEOS_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libgeos
LIBGEOS_MAKEFILE=$(LIBGEOS_SUBDIR)/Makefile

LIBGEOTIFF_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libgeotiff
LIBGEOTIFF_MAKEFILE=$(LIBGEOTIFF_SUBDIR)/Makefile

LIBJPEG_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libjpeg
LIBJPEG_MAKEFILE=$(LIBJPEG_SUBDIR)/Makefile

LIBPNG_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libpng
LIBPNG_CONFIGURE=$(LIBPNG_SUBDIR)/configure
LIBPNG_MAKEFILE=$(LIBPNG_SUBDIR)/Makefile

LIBTIFF_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libtiff
LIBTIFF_MAKEFILE=$(LIBTIFF_SUBDIR)/Makefile

LIBWEBP_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libwebp
LIBWEBP_MAKEFILE=$(LIBWEBP_SUBDIR)/Makefile

PROJ4_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/proj.4
PROJ4_MAKEFILE=$(PROJ4_SUBDIR)/Makefile

SQLITE_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/sqlite
SQLITE_MAKEFILE=$(SQLITE_SUBDIR)/Makefile

XZ_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/xz
XZ_MAKEFILE=$(XZ_SUBDIR)/Makefile

LIBMETAR_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/mdsplib
LIBMETAR_MAKEFILE=$(LIBMETAR_SUBDIR)/Makefile

LIBRASTERLITE2_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/librasterlite2
LIBRASTERLITE2_MAKEFILE=$(LIBRASTERLITE2_SUBDIR)/Makefile

LIBSPATIALITE_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libspatialite
LIBSPATIALITE_MAKEFILE=$(LIBSPATIALITE_SUBDIR)/Makefile

CURL_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/curl
CURL_MAKEFILE=$(CURL_SUBDIR)/Makefile

LIBXML2_SUBDIR=$(ROTOBOX_3RD_PARTY_DIR)/libxml2
LIBXML2_MAKEFILE=$(LIBXML2_SUBDIR)/Makefile


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

rotobox: rotobox.o gdl90.o database.o database_maintenance.o api.o \
		 $(DUMP978_DEPENDS) $(DUMP1090_DEPENDS) $(MONGOOSE_DEPENDS)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

rotobox-deps: libgeos librtlsdr libusb sqlite proj4 gpsd libmetar spatialite librasterlite2

clean:
	rm -rf *.o
	rm -rf rotobox

clean-deps: libgeos-clean librtlsdr-clean libusb-clean librasterlite2-clean spatialite-clean \
			sqlite-clean proj4-clean gpsd-clean libmetar-clean giflib-clean pixman-clean libcairo-clean \
			libgeotiff-clean libjpeg-clean libpng-clean libtiff-clean curl-clean xz-clean libxml2-clean

reset: clean libgeos-reset librtlsdr-reset libusb-reset librasterlite2-reset spatialite-reset \
	   sqlite-reset proj4-reset gpsd-reset libmetar-reset giflib-reset pixman-reset libcairo-reset \
	   libgeotiff-reset libjpeg-reset libpng-reset libtiff-reset curl-reset xz-reset libxml2-reset
	   rm -rf $(ROTOBOX_3RD_PARTY_BUILD_DIR)/*/


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
libusb: $(LIBUSB_MAKEFILE)
	$(MAKE) -C $(LIBUSB_SUBDIR) install

libusb-clean:
ifneq ("$(wildcard $(LIBUSB_MAKEFILE))","")
	$(MAKE) -C $(LIBUSB_SUBDIR) clean
endif

libusb-reset: libusb-clean
ifneq ("$(wildcard $(LIBUSB_MAKEFILE))","")
	rm $(LIBUSB_MAKEFILE)
endif

$(LIBUSB_MAKEFILE):
	cd $(LIBUSB_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./bootstrap.sh && sleep 2 && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# Librtlsdr                            #
########################################
librtlsdr: libusb $(LIBRTLSDR_MAKEFILE)
	$(MAKE) -C $(LIBRTLSDR_BUILDDIR) install

librtlsdr-clean:
ifneq ("$(wildcard $(LIBRTLSDR_MAKEFILE))","")
	$(MAKE) -C $(LIBRTLSDR_BUILDDIR) clean
endif

librtlsdr-reset: librtlsdr-clean
ifneq ("$(wildcard $(LIBRTLSDR_MAKEFILE))","")
	rm $(LIBRTLSDR_MAKEFILE)
endif

$(LIBRTLSDR_MAKEFILE):
	mkdir -p $(LIBRTLSDR_BUILDDIR) && rm -f $(LIBRTLSDR_BUILDDIR)/CMakeCache.txt
	cd $(LIBRTLSDR_BUILDDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	cmake ../ -DCMAKE_INSTALL_PREFIX=$(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# GIFLIB                               #
########################################
giflib: $(GIFLIB_MAKEFILE)
	$(MAKE) -C $(GIFLIB_SUBDIR) install

giflib-clean:
ifneq ("$(wildcard $(GIFLIB_MAKEFILE))","")
	$(MAKE) -C $(GIFLIB_SUBDIR) clean
endif

giflib-reset: giflib-clean
ifneq ("$(wildcard $(GIFLIB_MAKEFILE))","")
	rm $(GIFLIB_MAKEFILE)
endif

$(GIFLIB_MAKEFILE):
	cd $(GIFLIB_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# gpsd                                 #
########################################
gpsd-clean:
	cd $(GPSD_SUBDIR) && \
	scons -c && \
	scons sconsclean

gpsd-reset: gpsd-clean

gpsd:
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
	cd $(GPSD_SUBDIR) && \
	scons prefix=$(ROTOBOX_3RD_PARTY_BUILD_DIR) shared=False python_libdir=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/python --config=force && \
	scons install

########################################
# libcairo                             #
########################################
libcairo: pixman $(LIBCAIRO_MAKEFILE)
	$(MAKE) -C $(LIBCAIRO_SUBDIR) install

libcairo-clean:
ifneq ("$(wildcard $(LIBCAIRO_MAKEFILE))","")
	$(MAKE) -C $(LIBCAIRO_SUBDIR) clean
endif

libcairo-reset: libcairo-clean
ifneq ("$(wildcard $(LIBCAIRO_MAKEFILE))","")
	rm $(LIBCAIRO_MAKEFILE)
endif

$(LIBCAIRO_MAKEFILE):
	cd $(LIBCAIRO_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# pixman                               #
########################################
pixman: $(PIXMAN_MAKEFILE)
	$(MAKE) -C $(PIXMAN_SUBDIR) install

pixman-clean:
ifneq ("$(wildcard $(PIXMAN_MAKEFILE))","")
	$(MAKE) -C $(PIXMAN_SUBDIR) clean
endif

pixman-reset: pixman-clean
ifneq ("$(wildcard $(PIXMAN_MAKEFILE))","")
	rm $(PIXMAN_MAKEFILE)
endif

$(PIXMAN_MAKEFILE):
	cd $(PIXMAN_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	autoreconf -if -Wall && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libgeos                              #
########################################
libgeos: $(LIBGEOS_MAKEFILE)
	$(MAKE) -C $(LIBGEOS_SUBDIR) install

libgeos-clean:
ifneq ("$(wildcard $(LIBGEOS_MAKEFILE))","")
	$(MAKE) -C $(LIBGEOS_SUBDIR) clean
endif

libgeos-reset: libgeos-clean
ifneq ("$(wildcard $(LIBGEOS_MAKEFILE))","")
	rm $(LIBGEOS_MAKEFILE)
endif

$(LIBGEOS_MAKEFILE):
	cd $(LIBGEOS_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libgeotiff                           #
########################################
libgeotiff: libtiff $(LIBGEOTIFF_MAKEFILE)
	$(MAKE) -C $(LIBGEOTIFF_SUBDIR) install

libgeotiff-clean:
ifneq ("$(wildcard $(LIBGEOTIFF_MAKEFILE))","")
	$(MAKE) -C $(LIBGEOTIFF_SUBDIR) clean
endif

libgeotiff-reset: libgeotiff-clean
ifneq ("$(wildcard $(LIBGEOTIFF_MAKEFILE))","")
	rm $(LIBGEOTIFF_MAKEFILE)
endif

$(LIBGEOTIFF_MAKEFILE):
	cd $(LIBGEOTIFF_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libjpeg                              #
########################################
libjpeg: $(LIBJPEG_MAKEFILE)
	$(MAKE) -C $(LIBJPEG_SUBDIR) install

libjpeg-clean:
ifneq ("$(wildcard $(LIBJPEG_MAKEFILE))","")
	$(MAKE) -C $(LIBJPEG_SUBDIR) clean
endif

libjpeg-reset: libjpeg-clean
ifneq ("$(wildcard $(LIBJPEG_MAKEFILE))","")
	rm $(LIBJPEG_MAKEFILE)
endif

$(LIBJPEG_MAKEFILE):
	cd $(LIBJPEG_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)
	# Hacky, but libjpeg tries to install its man pages in an odd location.
	# Make the directory here.
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/man/man1

########################################
# libpng                               #
########################################
libpng: $(LIBPNG_MAKEFILE)
	$(MAKE) -C $(LIBPNG_SUBDIR) install

libpng-clean:
ifneq ("$(wildcard $(LIBPNG_MAKEFILE))","")
	$(MAKE) -C $(LIBPNG_SUBDIR) clean
endif

libpng-reset: libpng-clean
ifneq ("$(wildcard $(LIBPNG_MAKEFILE))","")
	rm $(LIBPNG_MAKEFILE)
endif

$(LIBPNG_CONFIGURE):
	cd $(LIBPNG_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh

$(LIBPNG_MAKEFILE): $(LIBPNG_CONFIGURE)
	cd $(LIBPNG_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libtiff                              #
########################################
libtiff: xz $(LIBTIFF_MAKEFILE)
	$(MAKE) -C $(LIBTIFF_SUBDIR) install

libtiff-clean:
ifneq ("$(wildcard $(LIBTIFF_MAKEFILE))","")
	$(MAKE) -C $(LIBTIFF_SUBDIR) clean
endif

libtiff-reset: libtiff-clean
ifneq ("$(wildcard $(LIBTIFF_MAKEFILE))","")
	rm $(LIBTIFF_MAKEFILE)
endif

$(LIBTIFF_MAKEFILE):
	cd $(LIBTIFF_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libwebp                              #
########################################
libwebp: $(LIBWEBP_MAKEFILE)
	$(MAKE) -C $(LIBWEBP_SUBDIR) install

libwebp-clean:
ifneq ("$(wildcard $(LIBWEBP_MAKEFILE))","")
	$(MAKE) -C $(LIBWEBP_SUBDIR) clean 
endif

libwebp-reset: libwebp-clean
ifneq ("$(wildcard $(LIBWEBP_MAKEFILE))","")
	rm $(LIBWEBP_MAKEFILE)
endif

$(LIBWEBP_MAKEFILE):
	cd $(LIBWEBP_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh && sleep 2 && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# proj.4                               #
########################################
proj4: $(PROJ4_MAKEFILE)
	$(MAKE) -C $(PROJ4_SUBDIR) install

proj4-clean:
ifneq ("$(wildcard $(PROJ4_MAKEFILE))","")
	$(MAKE) -C $(PROJ4_SUBDIR) clean
endif

proj4-reset: proj4-clean
ifneq ("$(wildcard $(PROJ4_MAKEFILE))","")
	  rm $(PROJ4_MAKEFILE)
endif

$(PROJ4_MAKEFILE):
	cd $(PROJ4_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# sqlite                               #
########################################
sqlite: $(SQLITE_MAKEFILE)
	$(MAKE) -C $(SQLITE_SUBDIR) install

sqlite-clean:
ifneq ("$(wildcard $(SQLITE_MAKEFILE))","")
	$(MAKE) -C $(SQLITE_SUBDIR) clean
endif

sqlite-reset: sqlite-clean
ifneq ("$(wildcard $(SQLITE_MAKEFILE))","")
	rm $(SQLITE_MAKEFILE)
endif

$(SQLITE_MAKEFILE):
	cd $(SQLITE_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --disable-tcl --enable-rtree

########################################
# xz                                   #
########################################
xz: $(XZ_MAKEFILE)
	$(MAKE) -C $(XZ_SUBDIR) install

xz-clean:
ifneq ("$(wildcard $(XZ_MAKEFILE))","")
	$(MAKE) -C $(XZ_SUBDIR) clean
endif

xz-reset: xz-clean
ifneq ("$(wildcard $(XZ_MAKEFILE))","")
	rm $(XZ_MAKEFILE)
endif

$(XZ_MAKEFILE):
	cd $(XZ_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libmetar                             #
########################################
libmetar:
	$(MAKE) -C $(LIBMETAR_SUBDIR) install

libmetar-clean:
	$(MAKE) -C $(LIBMETAR_SUBDIR) clean

libmetar-reset: libmetar-clean

########################################
# librasterlite2                       #
########################################
librasterlite2: curl libxml2 spatialite proj4 libpng giflib libwebp \
				libjpeg libgeotiff xz libcairo $(LIBRASTERLITE2_MAKEFILE)
	$(MAKE) -C $(LIBRASTERLITE2_SUBDIR)
	$(MAKE) -C $(LIBRASTERLITE2_SUBDIR) install

librasterlite2-clean:
ifneq ("$(wildcard $(LIBRASTERLITE2_MAKEFILE))","")
	$(MAKE) -C $(LIBRASTERLITE2_SUBDIR) clean
endif

librasterlite2-reset: xz-clean
ifneq ("$(wildcard $(LIBRASTERLITE2_MAKEFILE))","")
	rm $(LIBRASTERLITE2_MAKEFILE)
endif

$(LIBRASTERLITE2_MAKEFILE):
	cd $(LIBRASTERLITE2_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# spatialite                           #
########################################
spatialite: sqlite proj4 libgeos libxml2 $(LIBSPATIALITE_MAKEFILE)
	$(MAKE) -C $(LIBSPATIALITE_SUBDIR) install

spatialite-clean:
ifneq ("$(wildcard $(LIBSPATIALITE_MAKEFILE))","")
	$(MAKE) -C $(LIBSPATIALITE_SUBDIR) clean
endif

spatialite-reset: spatialite-clean
ifneq ("$(wildcard $(LIBSPATIALITE_MAKEFILE))","")
	rm $(LIBSPATIALITE_MAKEFILE)
endif

$(LIBSPATIALITE_MAKEFILE):
	cd $(LIBSPATIALITE_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --disable-freexl --with-geosconfig=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/bin/geos-config

########################################
# curl                                 #
########################################
curl: $(CURL_MAKEFILE)
	$(MAKE) -C $(CURL_SUBDIR) install

curl-clean:
ifneq ("$(wildcard $(CURL_MAKEFILE))","")
	$(MAKE) -C $(CURL_SUBDIR) clean
endif

curl-reset: curl-clean
ifneq ("$(wildcard $(CURL_MAKEFILE))","")
	rm $(CURL_MAKEFILE)
endif

$(CURL_MAKEFILE):
	cd $(CURL_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./buildconf && \
	./configure --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR)

########################################
# libxml2                              #
########################################
libxml2: $(LIBXML2_MAKEFILE)
	mkdir -p $(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
	$(MAKE) -C $(LIBXML2_SUBDIR) install

libxml2-clean:
ifneq ("$(wildcard $(LIBXML2_MAKEFILE))","")
	$(MAKE) -C $(LIBXML2_SUBDIR) clean
endif

libxml2-reset: libxml2-clean
ifneq ("$(wildcard $(LIBXML2_MAKEFILE))","")
	rm $(LIBXML2_MAKEFILE)
endif

$(LIBXML2_MAKEFILE):
	cd $(LIBXML2_SUBDIR) && \
	PKG_CONFIG_LIBDIR=$(ROTOBOX_PKG_CONFIG_PATH) \
	CPPFLAGS=-I$(ROTOBOX_3RD_PARTY_INCLUDE) \
	LDFLAGS=-L$(ROTOBOX_3RD_PARTY_LIB) \
	./autogen.sh --prefix $(ROTOBOX_3RD_PARTY_BUILD_DIR) --with-python-install-dir=$(ROTOBOX_3RD_PARTY_BUILD_DIR)/python
