export PWD=$(shell pwd)
BINNAME=$(notdir $(PWD))
MAKE += --no-print-directory
export CC=gcc
export CXX=g++
export DBG_FLAGS?=-O2
export LIBS=-lm -lstdc++ -lGL -lSDL2 -lGLU -lGLEW -lX11 -lXrandr -lpaho-mqtt3a
export LDFLAGS += -z noexecstack -z relro -z now -pie
export INCLUDES=
export CFLAGS=-c -Wall -DLINUX -std=c++11
export CFLAGS += -fstack-protector -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security
export HEADERS=$(wildcard *.h)
export SOURCES_C=$(wildcard *.c)
export SOURCES_CPP=$(wildcard *.cpp)
OBJECTS=$(SOURCES_C:.c=.o) $(SOURCES_CPP:.cpp=.o)

all::
	echo "Compiling..."

all::$(SOURCES_C) $(SOURCES_CPP) $(BINNAME)

all::
	echo "Done"

$(BINNAME): $(OBJECTS)
	@$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	@if [ "$(DBG_FLAGS)" = "-O2" ]; then \
		echo "Stripping $@"; \
		strip -x $@; \
	fi

.cpp.o: $(HEADERS)
	@echo $<
	@$(CXX) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $<

.c.o: $(HEADERS)
	@echo $<
	@$(CC) $(DBG_FLAGS) $(CFLAGS) $(INCLUDES) $<

debug:
	echo "Building binaries for debug..."
	DBG_FLAGS='-g -O0' $(MAKE)


install:
	@echo "Installing..."
	@if [ "$(DESTDIR)" != "$(PWD)" ]; then \
    		echo "Installing into DESTDIR"; \
    		cp $(BINNAME) $(DESTDIR)/$(BINNAME); \
    		cp -R $(PWD)/params/ $(DESTDIR); \
			cp -R $(PWD)/textures/ $(DESTDIR); \
    	fi

clean:
	@echo "Cleaning..."
	@rm -f $(BINNAME) *.o
	@rm -f $(DESTDIR)/$(BINNAME)
