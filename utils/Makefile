# Makefile - image_to_mem Xosera font conversion utility
# vim: set noet ts=8 sw=8

LDFLAGS		:= $(shell sdl2-config --libs) -lSDL2_image
SDL_CFLAGS	:= $(shell sdl2-config --cflags)

CFLAGS		:= -std=c++14 -Wall -Wextra -Werror -fomit-frame-pointer -Wno-sign-compare -Wno-unused-parameter $(SDL_CFLAGS)

image_to_mem: image_to_mem.cpp
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

clean:
	rm -f image_to_mem
