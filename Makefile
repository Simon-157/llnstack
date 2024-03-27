# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Isrc/include

# Directories
SRC_DIR := src
LIB_DIR := $(SRC_DIR)/lib
OBJ_DIR := obj
APP_DIR := apps
BIN_DIR := bin

# Source files
LIB_SRCS := $(wildcard $(LIB_DIR)/*.c)
LIB_OBJS := $(patsubst $(LIB_DIR)/%.c, $(OBJ_DIR)/%.o, $(LIB_SRCS))

HANDLER_DIR := handler
HANDLER_SRCS := $(wildcard $(HANDLER_DIR)/*.c)
HANDLER_OBJS := $(patsubst $(HANDLER_DIR)/%.c, $(OBJ_DIR)/%.o, $(HANDLER_SRCS))

DEVICE_DIR := device
DEVICE_SRCS := $(DEVICE_DIR)/loopback.c
DEVICE_OBJS := $(patsubst $(DEVICE_DIR)/%.c, $(OBJ_DIR)/%.o, $(DEVICE_SRCS))

APP_SRCS := $(wildcard $(APP_DIR)/*.c)
APPS := $(patsubst $(APP_DIR)/%.c, $(BIN_DIR)/%, $(APP_SRCS))

# Targets
.PHONY: all clean

all: $(BIN_DIR) $(OBJ_DIR) $(LIB_OBJS) $(HANDLER_OBJS) $(DEVICE_OBJS) $(APPS)

$(OBJ_DIR)/%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(HANDLER_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(DEVICE_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(APP_DIR)/%.c $(LIB_OBJS) $(HANDLER_OBJS) $(DEVICE_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) $(HANDLER_OBJS) $(DEVICE_OBJS) -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

