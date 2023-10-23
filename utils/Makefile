# $(MAKEFILE_LIST) - image_to_mem Xosera font conversion utility
# vim: set noet ts=8 sw=8

# Makefile "best practices" from https://tech.davis-hansson.com/p/make/ (but not forcing gmake)
SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c
.ONESHELL:
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

LDFLAGS		:= $(shell sdl2-config --libs) -lSDL2_image
SDL_CFLAGS	:= $(shell sdl2-config --cflags)

CXXFLAGS	:= -Os -std=c++20 -Wall -Wextra -Werror $(SDL_CFLAGS)

all: $(basename $(wildcard *.cpp))

clean:
	rm -f $(basename $(wildcard *.cpp))

% : %.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

.PHONY: all clean
