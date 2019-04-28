# GE's makefile for Linux
# Mempuntu - 19/04/2019

print-%  : ; @echo $* = $($*)
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
CL_FLAGS=-std=c++17 -O2 -fpermissive

# linking compiler calls and flags
LINK=g++
LINK_FLAGS=-ldl -lm -lSDL2 -lSDL2main

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

HEADERS := $(wildcard src/headers/*.h)
HEADERS += $(GL3W_H) $(IMGUI_H)

SOURCES := $(wildcard src/*.cpp)
SOURCES += $(IMGUI_C)

OBJS := $(SOURCES:.cpp=.o)
OUTPUT_OBJS := $(addprefix $(BUILD_PATH),$(notdir $(OBJS))) $(GL3W_O)

.PHONY: build rebuild clean

build:
	@echo "\n Compilation $(GREEN)started$(NC) at $$(date +%T). \n"
	make $(EXE) -j3
	@echo "\n Compilation $(GREEN)finished$(NC) at $$(date +%T). \n"

clean:
	rm -fr $(addprefix $(BUILD_PATH), $(EXE)) $(OUTPUT_OBJS)
	@echo "\n Directory $(GREEN)cleaned$(NC) succesfully. \n"

rebuild: clean build

run:
	@echo "\n running $(EXE) .. \n"
	./$(addprefix $(BUILD_PATH), $(EXE))

$(EXE): $(OUTPUT_OBJS)
	   $(LINK) -o $(addprefix $(BUILD_PATH), $(EXE)) $(OUTPUT_OBJS) $(LINK_FLAGS)

VPATH= $(sort $(dir $(SOURCES)))

$(BUILD_PATH)%.o: %.cpp
		$(CL) $(CL_FLAGS) $(INC) $< -o $@

$(BUILD_PATH)gl3w.o: $(GL3W_C) $(GL3W_H)
		$(CL) $(CL_FLAGS) $(INC) $(GL3W_C) -o $(GL3W_O)


