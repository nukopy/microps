SRC_DIR := src
TEST_DIR := test

APPS =

DRIVERS = $(SRC_DIR)/driver/dummy.o

OBJS = $(SRC_DIR)/util.o \
	   $(SRC_DIR)/net.o \

TESTS = $(TEST_DIR)/step0.exe \
        $(TEST_DIR)/step1.exe \
        $(TEST_DIR)/step2.exe \

CC := gcc
CFLAGS := $(CFLAGS) -g -W -Wall -Wno-unused-parameter -iquote $(SRC_DIR)

ifeq ($(shell uname),Linux)
  # Linux specific settings
  BASE = $(SRC_DIR)/platform/linux
  CFLAGS := $(CFLAGS) -pthread -iquote $(BASE)
  OBJS := $(OBJS) $(BASE)/intr.o
endif

ifeq ($(shell uname),Darwin)
  # macOS specific settings
endif

.SUFFIXES:
.SUFFIXES: .c .o

.PHONY: all clean format

all: $(APPS) $(TESTS)

$(APPS): %.exe : %.o $(OBJS) $(DRIVERS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TESTS): %.exe : %.o $(OBJS) $(DRIVERS) $(TEST_DIR)/test.h
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(APPS) $(APPS:.exe=.o) $(OBJS) $(DRIVERS) $(TESTS) $(TESTS:.exe=.o)

format:
	find . -name "*.c" -o -name "*.h" | xargs clang-format -i --sort-includes
