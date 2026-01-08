# Configuration 

CC=			 gcc
LD=			 gcc
CFLAGS=		 -Wall -Wextra -Wpedantic -g -Og
LDFLAGS=	 -Lbuild 

# Files 

GIT_HEADERS= $(wildcard include/*.h)
GIT_SOURCES= $(filter-out src/git_clone.c,$(wildcard src/*.c))
GIT_OBJECTS= $(patsubst src/%.c,build/%.o,$(GIT_SOURCES))
GIT_INCLUDES=-Iinclude

GIT_MAIN_SRC= src/git_clone.c
GIT_MAIN_OBJ= $(patsubst src/%.c,build/%.o,$(GIT_MAIN_SRC))
GIT_PROGRAM= bin/git_clone 

GIT_TEST_SRCS=  $(wildcard test/*.c)
GIT_TEST_OBJS=  $(patsubst test/%.c,build/%.o,$(GIT_TEST_SRCS))  
GIT_UNIT_TESTS= $(patsubst build/%.o,bin/%,$(GIT_TEST_OBJS))

# Rules 

all: $(GIT_PROGRAM) $(GIT_UNIT_TESTS)

.SECONDARY: $(GIT_OBJECTS) $(GIT_MAIN_OBJ) $(GIT_TEST_OBJS)

bin:
	@echo "making bin directory"
	@mkdir -p $@ 

build:
	@echo "making build directory"
	@mkdir -p $@ 

$(GIT_PROGRAM): $(GIT_MAIN_OBJ) $(GIT_OBJECTS) | bin 
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) $^ -o $@ 

build/%.o: src/%.c $(GIT_HEADERS) | build 
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) $(GIT_INCLUDES) -c $< -o $@ 

build/unit_%.o: test/unit_%.c $(GIT_HEADERS) | build
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) $(GIT_INCLUDES) -c $< -o $@

bin/unit_%: build/unit_%.o $(GIT_OBJECTS) | bin
	@echo "Linking $@"
	@$(LD) $(LDFLAGS) $^ -o $@ 

clean:
	@echo "Removing Objects"
	@rm -f $(GIT_OBJECTS) $(GIT_TEST_OBJS) $(GIT_MAIN_OBJ)

	@echo "Remvoing git clone"
	@rm -f $(GIT_PROGRAM)

	@echo "Removing Tests"
	@rm -f $(GIT_UNIT_TESTS)

.PHONY: clean all 