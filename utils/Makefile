# Makefile - image_to_mem Xosera font conversion utility
# vim: set noet ts=8 sw=8

LDFLAGS		:= $(shell sdl2-config --libs) -lSDL2_image
SDL_CFLAGS	:= $(shell sdl2-config --cflags)

CFLAGS		:= -Os -std=c++14 -Wall -Wextra -Werror $(SDL_CFLAGS)

all: true_color_hack image_to_mem image_pal image_to_monobitmap raw256to16color pal_to_raw interleave_raw

image_to_mem: Makefile image_to_mem.cpp
	$(CXX) $(CFLAGS) image_to_mem.cpp -o image_to_mem $(LDFLAGS)

image_pal: Makefile image_pal.cpp
	$(CXX) $(CFLAGS) image_pal.cpp -o image_pal $(LDFLAGS)

image_to_monobitmap: Makefile image_to_monobitmap.cpp
	$(CXX) $(CFLAGS) image_to_monobitmap.cpp -o image_to_monobitmap $(LDFLAGS)

raw256to16color: Makefile raw256to16color.cpp
	$(CXX) $(CFLAGS) raw256to16color.cpp -o raw256to16color $(LDFLAGS)

true_color_hack: Makefile true_color_hack.cpp
	$(CXX) $(CFLAGS) true_color_hack.cpp -o true_color_hack $(LDFLAGS)

interleave_raw: Makefile interleave_raw.cpp
	$(CXX) $(CFLAGS) interleave_raw.cpp -o interleave_raw $(LDFLAGS)

#WIP
pal_to_raw: Makefile pal_to_raw.cpp
	$(CXX) $(CFLAGS) pal_to_raw.cpp -o pal_to_raw $(LDFLAGS)

clean:
	rm -f image_to_mem image_pal image_to_monobitmap raw256to16color pal_to_raw

.PHONY: all clean
