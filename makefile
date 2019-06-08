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
-isystem dependencies/glm \
-isystem dependencies/imgui \
-isystem dependencies/SDL/include

# compilation calls and flags
CL=g++ -c
CL_FLAGS=`pkg-config gtk+-2.0 --cflags --libs` -std=c++17

# linking compiler calls and flags
LINK=g++
LINK_FLAGS=-ldl -lm -lSDL2 -lSDL2main `pkg-config gtk+-2.0 --cflags --libs`

#program name and source files
EXE=GE
BUILD_PATH=linux_build/

# Library headers, cpp and o files
IMGUI_H := $(wildcard dependencies/imgui/*.h)
IMGUI_C := $(wildcard dependencies/imgui/*.cpp)
IMGUI_O := $(IMGUI_C:.cpp=.o)

GL3W_H := $(wildcard dependencies/gl3w/include/GL/*.h)
GL3W_C = dependencies/gl3w/src/gl3w.c
GL3W_O = $(addprefix $(BUILD_PATH),$(notdir $(GL3W_C:.c=.o)))

SOURCES := $(wildcard src/*.cpp) $(IMGUI_C)
OBJS := $(addprefix $(BUILD_PATH),$(notdir $(SOURCES:.cpp=.o))) $(GL3W_O)

.PHONY: build rebuild clean

build:
	make $(EXE) -j3
	@echo "\n Compilation $(GREEN)finished$(NC) at $$(date +%T). \n"

clean:
	rm -fr $(addprefix $(BUILD_PATH), $(EXE)) $(OBJS)
	@echo "\n Directory $(GREEN)cleaned$(NC) succesfully. \n"

rebuild: clean build

run:
	@echo "\n running $(EXE) .. \n"
	./$(addprefix $(BUILD_PATH), $(EXE))

$(EXE): $(OBJS)
	   $(LINK) -o $(addprefix $(BUILD_PATH), $(EXE)) $^ $(LINK_FLAGS)

#determines the directories to scan for prerequisites
VPATH= $(sort $(dir $(SOURCES)))

$(BUILD_PATH)%.o: %.cpp
		$(CL) $(CL_FLAGS) $(INC) $< -o $@

$(BUILD_PATH)gl3w.o: $(GL3W_C) $(GL3W_H)
		$(CL) $(CL_FLAGS) $(INC) $(GL3W_C) -o $(GL3W_O)


