OBJS = $(wildcard src/*.c)

UNIX_CC = gcc 
WIN_CC = gcc

UNIX_COMPILER_FLAGS = -w -std=c99 -g
WIN_COMPILER_FLAGS = -w -std=c99

ifeq ($(OS),Windows_NT)
	SHELL = cmd.exe
	MAKE_DIR = $(shell cd)
else
	MAKE_DIR = $(shell pwd)
endif

INCLUDES = -I$(MAKE_DIR)/dep/ -I$(MAKE_DIR)/dep/openal-soft/include/

UNIX_LINKER_FLAGS = -L$(MAKE_DIR)/dep/openal-soft/ -lopenal -lm
WIN_LINKER_FLAGS = -L$(MAKE_DIR)/dep/openal-soft/ -lopenal32 -lm
OSX_LINKER_FLAGS = -L$(MAKE_DIR/dep/openal-soft/ -lopenal -lm

UNIX_EXEC_NAME = al 
WIN_EXEC_NAME = $(UNIX_EXEC_NAME).exe
OSX_EXEC_NAME = $(UNIX_EXEC_NAME).app

ifeq ($(OS),Windows_NT)
	RM_CMD = -del
	TARGET_LINKER_FLAGS := $(WIN_LINKER_FLAGS)
	TARGET_COMPILER_FLAGS := $(WIN_COMPILER_FLAGS)
	TARGET_CC := $(WIN_CC)
	TARGET_EXEC_NAME := $(WIN_EXEC_NAME)
	TARGET_COMPILER_FLAGS += -D WIN32

	ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
		TARGET_COMPILER_FLAGS += -march=x86-64
	else ifeq ($(PROCESSOR_ARCHITECTURE),x86)
		TARGET_COMPILER_FLAGS += -march=i386
	endif
else
	UNAME_S := $(shell uname -s)
	UNAME_P := $(shell uname -p)
	RM_CMD = -rm
	
	ifeq ($(UNAME_S),Linux)
		TARGET_CC := $(UNIX_CC)
		TARGET_COMPILER_FLAGS := $(UNIX_COMPILER_FLAGS)
		TARGET_LINKER_FLAGS := $(UNIX_LINKER_FLAGS)
		TARGET_EXEC_NAME := $(UNIX_EXEC_NAME)
		TARGET_COMPILER_FLAGS += -D LINUX
	else ifeq ($(UNAME_S),Darwin)
		TARGET_CC := $(UNIX_CC)
		TARGET_COMPILER_FLAGS := $(UNIX_COMPILER_FLAGS)
		TARGET_LINKER_FLAGS := $(OSX_LINKER_FLAGS)
		TARGET_EXEC_NAME := $(OSX_EXEC_NAME)
		TARGET_COMPILER_FLAGS += -D OSX
	endif

	#Default to x86_64
	ifeq ($(UNAME_P),x86_64)
		TARGET_COMPILER_FLAGS += -march=x86-64
	else ifeq ($(UNAME_P),x86)
		TARGET_COMPILER_FLAGS += -march=i386
	else ifeq ($(UNAME_P),ARM)
		TARGET_COMPILER_FLAGS += -march=ARM
	else ifeq ($(UNAME_P),ARM64)
		TARGET_COMPILER_FLAGS += -march=ARM64
	else ifeq ($(UNAME_P),aarch64)
		TARGET_COMPILER_FLAGS == -march=ARM64
	else
		TARGET_COMPILER_FLAGS += -march=x86-64
	endif

endif

all : $(OBJS)
	$(TARGET_CC) $(OBJS) $(TARGET_COMPILER_FLAGS) $(TARGET_LINKER_FLAGS) $(INCLUDES) -o $(TARGET_EXEC_NAME)

.PHONY: clean
clean :
	$(RM_CMD) $(TARGET_EXEC_NAME)
