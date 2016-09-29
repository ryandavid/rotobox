#CFLAGS+=-O2 -g -Wall -Werror -Ifec
CFLAGS+=-O2 -g -Wall -Ifec -Wno-unknown-pragmas
LDFLAGS=
LIBS=-lm -lpthread -ldl -lgps -lgeos_c `pkg-config --libs librtlsdr libusb-1.0 spatialite rasterlite2 sqlite3 proj`
CC=gcc
MAKE=make
CMAKE=cmake
ECHO=echo
UNAME=$(shell uname -s)
CPPFLAGS= -I./dump978 -I./dump1090 -I./mongoose -I./ -DMAKE_DUMP_978_LIB -DMAKE_DUMP_1090_LIB `pkg-config --cflags librtlsdr libusb-1.0 spatialite rasterlite2 sqlite3 proj`

DUMP978_SUBDIR=dump978
DUMP1090_SUBDIR=dump1090
LIBRTLSDR_SUBDIR=librtlsdr
LIBRTLSDR_BUILDDIR=$(LIBRTLSDR_SUBDIR)/build
LIBRTLSDR_MAKEFILE=$(LIBRTLSDR_BUILDDIR)/Makefile

DUMP978_DEPENDS=dump978/dump978.o dump978/uat_decode.o dump978/fec.o dump978/fec/init_rs_char.o \
				dump978/fec/decode_rs_char.o
DUMP1090_DEPENDS=dump1090/dump1090.o dump1090/convert.o dump1090/anet.o dump1090/cpr.o \
				dump1090/demod_2400.o dump1090/icao_filter.o dump1090/interactive.o \
				dump1090/mode_ac.o dump1090/mode_s.o dump1090/net_io.o dump1090/stats.o \
				dump1090/track.o dump1090/util.o dump1090/crc.o

MONGOOSE_DEPENDS=mongoose/mongoose.o

MDSPLIB_DEPENDS=mdsplib/src/decode_metar.o mdsplib/src/antoi.o mdsplib/src/charcmp.o \
				mdsplib/src/decode_metar_remark.o mdsplib/src/fracpart.c mdsplib/src/print_decoded_metar.o \
				mdsplib/src/stspack2.o mdsplib/src/stspack3.o

ALL_SUBDIRS=$(LIBRTLSDR_BUILDDIR) $(DUMP978_SUBDIR) $(DUMP1090_SUBDIR)

ifeq ($(UNAME), Darwin)
# TODO: Putting GCC in C11 mode breaks things.
CFLAGS+=-std=c11 -DMISSING_GETTIME -DMISSING_NANOSLEEP
DUMP1090_DEPENDS+=dump1090/compat/clock_gettime/clock_gettime.o dump1090/compat/clock_nanosleep/clock_nanosleep.o
endif

all: librtlsdr dump978 dump1090 rotobox
.PHONY: librtlsdr dump978 dump1090

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

rotobox: rotobox.o gdl90.o database.o api.o $(DUMP978_DEPENDS) $(DUMP1090_DEPENDS) $(MONGOOSE_DEPENDS) $(MDSPLIB_DEPENDS)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

librtlsdr: $(LIBRTLSDR_MAKEFILE)
	$(ECHO) "Building librtlsdr"
	$(MAKE) -C $(LIBRTLSDR_BUILDDIR)

$(LIBRTLSDR_MAKEFILE):
	$(ECHO) "Need to run cmake for librtlsdr"
	mkdir $(LIBRTLSDR_BUILDDIR)
	cmake -B$(LIBRTLSDR_BUILDDIR) -H$(LIBRTLSDR_SUBDIR) -DINSTALL_UDEV_RULES=ON

dump978:
	$(ECHO) "Building dump978"
	$(MAKE) -C $(DUMP978_SUBDIR)

dump1090:
	$(ECHO) "Building dump1090"
	$(MAKE) -C $(DUMP1090_SUBDIR)

clean:
	@for dir in $(ALL_SUBDIRS); do \
		$(ECHO) "Cleaning up $$dir"; \
		$(MAKE) -C $$dir clean; \
	done

	rm -rf $(LIBRTLSDR_BUILDDIR)

	rm -rf rotobox


install: all
	$(ECHO) "Need to install librtlsdr"
	sudo $(MAKE) -C $(LIBRTLSDR_BUILDDIR) install

	@if [ `uname -s` == Linux ]; then \
		$(ECHO) "Running ldconfig"; \
		sudo ldconfig; \
	fi