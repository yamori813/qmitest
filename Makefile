# Makefile for gmake

PROGRAM = getmodel getprofilelist getprofilesetting getsig getsys

LIBUSB = ../../../OpenSource/libusb-1.0.9/libusb
DYLIB = libusb-1.0.0.dylib

all:	libusb.dylib $(PROGRAM)

LDFLAGS = -L. -lusb
CPPFLAGS = -I$(LIBUSB)

%: %.cc
	$(LINK.cc) $^ $(LOADLIBES) QMI.c $(LDLIBS) -o $@

libusb.dylib : 
	cp $(LIBUSB)/.libs/$(DYLIB) .
	install_name_tool -id @executable_path/$(DYLIB) $(DYLIB)
	ln -s $(DYLIB) libusb.dylib

clean:
	rm -rf $(PROGRAM) libusb.dylib $(DYLIB)
