TARGET = dev

# Compiler options.
CC_DEV = g++
CC_ARM = /usr/bin/arm-linux-gnueabihf-g++

# Header includes.
INCLUDEPATH = -Idkm/include -Istb

# Flags.
CFLAGS = -Wall -static -O3 -std=c++14 

# Dependency.
#LDFLAGS = -lboost_serialization

# Source directory and files.
SOURCEDIR = src
HEADERS := $(wildcard $(SOURCEDIR)/*.hpp)
SOURCES := $(wildcard $(SOURCEDIR)/*.cpp)

# Target output.
BUILDTARGET = K_Means

# Target compiler environment.
ifeq ($(TARGET),arm)
	CC = $(CC_ARM)
else
	CC = $(CC_DEV)
endif

all:
	$(CC) $(CFLAGS) $(INCLUDEPATH) $(HEADERS) $(SOURCES) -o $(BUILDTARGET)
#	$(CC) $(CFLAGS) $(INCLUDEPATH) $(HEADERS) $(SOURCES) -o $(BUILDTARGET) $(LDFLAGS)

clean:
	rm -f $(SOURCEDIR)/*.o
	rm -f $(BUILDTARGET)