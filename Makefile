# Configuration 

CC=			 gcc
LD=			 gcc
CFLAGS=		 -Wall -Wextra -Wpedantic -g -Og
LDFLAGS=	 -Lbuild 

# Files 

GIT_HEADERS= $(wildcard include/*.h)
GIT_SOURCES= $(wildcard src/*.c)
GIT_OBJECTS= $(patsubst src/%.c,build/%.o,$(GIT_SOURCES))
GIT_INCLUDES=-Iinclude
GIT_PROGRAM= bin/git_clone 

# Rules 

all: $(GIT_PROGRAM)

bin:
	@echo "making bin directory"
	@mkdir -p $@ 

build:
	@echo "making build directory"
	@mkdir -p $@ 

$(GIT_PROGRAM): $(GIT_OBJECTS) | bin 
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) $^ -o $@ 

build/%.o: src/%.c $(GIT_HEADERS) | build 
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) $(GIT_INCLUDES) -c $< -o $@ 

clean:
	@echo "Removing Objects"
	@rm -f $(GIT_OBJECTS)

	@echo "Remvoing git clone"
	@rm -f $(GIT_PROGRAM)

.PHONY: clean all 