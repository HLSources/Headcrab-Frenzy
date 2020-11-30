#
# hlds (front end for hlds_l) Makefile for Linux i386
#
# May 2000, Leon Hartwig (hartwig@valvesoftware.com)
#

#make sure this is the correct compiler for your system
CC=gcc-linux-2.95

SRCDIR=.
OBJDIR=$(SRCDIR)/obj-$(ARCH)

#safe optimization
CFLAGS=-w -mcpu=$(ARCH) -O1

#full optimization
#CFLAGS=-w -mcpu=$(ARCH) -O2 \
	-ffast-math -funroll-loops \
	-fexpensive-optimizations -malign-loops=2 \
	-malign-jumps=2 -malign-functions=2

#use these when debugging 
#CFLAGS=$(BASE_CFLAGS) -g

LDFLAGS=-lgcc -ldl /opt/crosstool/i686-unknown-linux-gnu/gcc-2.95.3-glibc-2.1.3/lib/gcc-lib/i686-unknown-linux-gnu/2.95.3/libstdc++.a -lm

AR=ar
RANLIB=ranlib

INCLUDEDIRS=-I. -I../common

DO_CC=$(CC) $(INCLUDEDIRS) $(CFLAGS) -o $@ -c $<

#############################################################################
# HLDS FRONT END
#############################################################################

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(DO_CC)

OBJ = \
	$(OBJDIR)/sys_ded.o \
	$(OBJDIR)/engine.o

hlds-$(ARCH) : neat $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)
neat:
	-mkdir $(OBJDIR)
clean:
	-rm -f $(OBJ)
	-rm -f hlds-$(ARCH)
spotless: clean
	-rm -r $(OBJDIR)

