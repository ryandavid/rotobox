CFLAGS+=-O2 -g -Wall -Werror -Ifec
LDFLAGS=
LIBS=-lm
LIBS_RTL=`pkg-config --libs librtlsdr libusb-1.0`
CC=gcc
MAKE=make
CMAKE=cmake
ECHO=echo
UNAME=$(shell uname -s)

DUMP978_SUBDIR=dump978
DUMP1090_SUBDIR=dump1090
LIBRTLSDR_SUBDIR=librtlsdr
LIBRTLSDR_BUILDDIR=$(LIBRTLSDR_SUBDIR)/build
LIBRTLSDR_MAKEFILE=$(LIBRTLSDR_BUILDDIR)/Makefile

ALL_SUBDIRS=$(LIBRTLSDR_BUILDDIR) $(DUMP978_SUBDIR) $(DUMP1090_SUBDIR)

all: librtlsdr dump978 dump1090
.PHONY: librtlsdr dump978 dump1090

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


install: all
	$(ECHO) "Need to install librtlsdr"
	sudo $(MAKE) -C $(LIBRTLSDR_BUILDDIR) install

	@if [ `uname -s` == Linux ]; then \
		$(ECHO) "Running ldconfig"; \
		sudo ldconfig; \
	fi