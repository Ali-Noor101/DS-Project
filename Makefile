.PHONY: all clean rebuild run

BUILD_DIR = build
EXECUTABLE = $(BUILD_DIR)/xonix

all: $(EXECUTABLE)

$(EXECUTABLE): CMakeLists.txt Source.cpp
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake ..
	@cmake --build $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean all

run: $(EXECUTABLE)
	@cd $(BUILD_DIR) && ./xonix

