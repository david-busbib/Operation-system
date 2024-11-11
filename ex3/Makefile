# Compiler
CC=g++
CXX=g++
# Archiver
AR=ar
ARFLAGS=rcs
# Utility
RM=rm -f
# Ranlib
RANLIB=ranlib

# Source files
LIBSRC=MapReduceFramework.cpp Barrier.cpp
LIBOBJ=$(LIBSRC:.cpp=.o)

# Include directories
INCS=-I.
# Compiler flags
CFLAGS=-Wall -std=c++11 -g $(INCS)
CXXFLAGS=-Wall -std=c++11 -g $(INCS)

# Library target
OSMLIB=libMapReduceFramework.a
TARGETS=$(OSMLIB)

# Tar utility
TAR=tar
TARFLAGS=-cvf
TARNAME=ex1.tar
TARSRCS=$(LIBSRC) Makefile README

# Default target
all: $(TARGETS)

$(OSMLIB): $(LIBOBJ)
	$(AR) $(ARFLAGS) $@ $(LIBOBJ)
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(LIBOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

# Compilation rules
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Remove dependency lines from Makefile
clean_dep:
	$(RM) Makefile.bak

# DO NOT DELETE
