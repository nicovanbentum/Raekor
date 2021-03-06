# GE's makefile for Linux
# Mempuntu - 19/04/2019

#helper function that prints a variable
print-%  : ; @echo $* = $($*)

#ANSI color escape strings
RED=\033[0;31m
GREEN=\033[1;32m
NC=\033[0m

#include directories for header-only dependencies
INC=-isystem src/headers \
-isystem dependencies/gl3w/include \
-isystem dependencies/JSON/include \
-isystem dependencies/glm/glm \
-isystem dependencies/imgui \
-isystem dependencies/imgui/examples \
-isystem dependencies/cereal/include \
-isystem src/platform \
-isystem dependencies/stb

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LDFLAGS := $(shell sdl2-config --libs)

# this is to locate the assimp lib files
export LD_LIBRARY_PATH=/usr/local/lib

# compilation calls and flags
CL=g++ -c
CL_FLAGS=`pkg-config gtk+-2.0 --cflags --libs` -std=c++17

# linking compiler calls and flags
LINK=g++
LINK_FLAGS=-ldl -lm -lassimp `pkg-config gtk+-2.0 --cflags --libs`

#program name and source files
EXE=GE
OUT_DIR=x64/Linux/
MAKE_DIR= mkdir -p $(OUT_DIR)
DEL_DIR= rm -r -f $(OUT_DIR)

# Library headers, cpp and o files
IMGUI_H := $(wildcard dependencies/imgui/*.h) dependencies/imgui/examples/imgui_impl_opengl3.h \
dependencies/imgui/examples/imgui_impl_sdl.h
IMGUI_C := $(wildcard dependencies/imgui/*.cpp) dependencies/imgui/examples/imgui_impl_opengl3.cpp \
dependencies/imgui/examples/imgui_impl_sdl.cpp
IMGUI_O := $(IMGUI_C:.cpp=.o)

GL3W_H := $(wildcard dependencies/gl3w/include/GL/*.h)
GL3W_C = dependencies/gl3w/src/gl3w.c
GL3W_O = $(addprefix $(OUT_DIR),$(notdir $(GL3W_C:.c=.o)))

SOURCES := $(wildcard src/*.cpp) $(IMGUI_C) $(wildcard src/platform/linux/*.cpp)
OBJS := $(addprefix $(OUT_DIR),$(notdir $(SOURCES:.cpp=.o))) $(GL3W_O)

.PHONY: build rebuild clean

build:
	$(MAKE_DIR)
	make $(EXE) -j3
	@echo "\n Compilation $(GREEN)finished$(NC) at $$(date +%T). \n"

clean:
	$(DEL_DIR)
	@echo "\n Directory $(GREEN)cleaned$(NC) succesfully. \n"

rebuild: clean build

run:
	@echo "\n running $(EXE) .. \n"
	./$(addprefix $(OUT_DIR), $(EXE))

$(EXE): $(OBJS)
	   $(LINK) -o $(addprefix $(OUT_DIR), $(EXE)) $^ $(LINK_FLAGS) $(SDL_LDFLAGS)

#determines the directories to scan for prerequisites
VPATH= $(sort $(dir $(SOURCES)))

$(OUT_DIR)%.o: %.cpp
		$(CL) $(CL_FLAGS) $(SDL_CFLAGS) $(INC) $< -o $@ 

$(OUT_DIR)gl3w.o: $(GL3W_C) $(GL3W_H)
		$(CL) $(CL_FLAGS) $(INC) $(GL3W_C) -o $(GL3W_O)


