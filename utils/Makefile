# Makefile - image_to_mem Xosera font conversion utility
# vim: set noet ts=8 sw=8

LDFLAGS		:= $(shell sdl2-config --libs) -lSDL2_image
SDL_CFLAGS	:= $(shell sdl2-config --cflags)

CFLAGS		:= -Os -std=c++14 -Wall -Wextra -Werror $(SDL_CFLAGS)

all: image_to_mem image_to_monobitmap

image_to_mem: Makefile image_to_mem.cpp
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

image_to_monobitmap: Makefile image_to_monobitmap.cpp
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

clean:
	rm -f image_to_mem image_to_monobitmap

.PHONY:image_to_mem image_to_monobitmap
