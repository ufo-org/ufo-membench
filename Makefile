# You can set UFO_DEBUG=1 or UFO_DEBUG=0 in the environment to compile with or
# without debug symbols (this affects both the C and the Rust code).

SOURCES_C = src/membench.c

UFO_C_PATH=src/ufo_c

# -----------------------------------------------------------------------------

# cargo will generate the output binaries for ufo-c either in a debug
# subdirectory or a release subdirectory. We remember which.
ifeq (${UFO_DEBUG}, 1)
	CARGOFLAGS=+nightly build
	LIB_SUBDIR=/target/debug
else
	CARGOFLAGS=+nightly build --release
	LIB_SUBDIR=/target/release
endif

UFO_LIBS=$(UFO_C_PATH)/$(LIB_SUBDIR)/libufo_c.a
UFO_INCLUDES=-I$(UFO_C_PATH)/target/
COMMON_LIBS = -Wl,--no-as-needed -lpthread -lrt -ldl -lm -lstdc++
LIBS = $(COMMON_LIBS) $(UFO_LIBS)
COMMON_FLAGS = -fPIC -Wall -Werror $(UFO_INCLUDES)

ifeq (${UFO_DEBUG}, 1)
	CFLAGS =   -Og -ggdb $(COMMON_FLAGS) -DMAKE_SURE -DNDEBUG
	CXXFLAGS = -Og -ggdb $(COMMON_FLAGS)
else
	CFLAGS =   -O2 $(COMMON_FLAGS)
	CXXFLAGS = -O2 $(COMMON_FLAGS)
endif

# TODO split CFLAGS for different wotsits

# -----------------------------------------------------------------------------

.PHONY: all ufo-c ufo-c-clean clean

all: libs membench 

#UFO_OBJECTS = $(UFO_SOURCES_C:.c=.o)
OBJECTS = $(SOURCES_C:.c=.o)

libs: ufo-c $(OBJECTS)

membench: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o membench $(OBJECTS) $(LFLAGS) $(LIBS)

clean: ufo-c-clean 
	$(RM) src/*.o *~ membench 

ufo-c:
	cargo $(CARGOFLAGS) --manifest-path=$(UFO_C_PATH)/Cargo.toml

ufo-c-clean:
	cargo clean --manifest-path=$(UFO_C_PATH)/Cargo.toml

# update-dependencies:
# 	cd src/ufo_c && git pull	
