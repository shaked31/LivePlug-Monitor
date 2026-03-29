
CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -ldl

PLUGIN_DIR = plugins
BIN_PLUGIN_DIR = bin/plugins
CORE_DIR = core
BUILD_DIR = build

PLUGIN_SOURCES = $(wildcard $(PLUGIN_DIR)/*.c)
PLUGIN_SO = $(patsubst $(PLUGIN_DIR)/%.c, $(BIN_PLUGIN_DIR)/%.so, $(PLUGIN_SOURCES))
CODE_SOURCES = $(CORE_DIR)/main.c $(CORE_DIR)/core_engine.c $(CORE_DIR)/utils.c

all: setup core plugins

clean: clean_plugins clean_build

setup:
	@mkdir -p $(BIN_PLUGIN_DIR)
	@mkdir -p $(BUILD_DIR)

core: setup
	$(CC) $(CFLAGS) $(CODE_SOURCES) -o $(BUILD_DIR)/core $(LDFLAGS)

plugins: $(PLUGIN_SO)

$(BIN_PLUGIN_DIR)/%.so: $(PLUGIN_DIR)/%.c setup
	$(CC) $(CFLAGS) -fPIC -shared $< -o $@

clean_plugins:
	rm -rf bin/plugins/*

clean_build:
	rm -rf build/*

.PHONY: all clean setup core plugins clean_plugins clean_build