# ============================================================================
#  CivicShield Disaster Management System — Makefile (Linux / macOS / MSYS2)
# ----------------------------------------------------------------------------
#  Windows users: use the MinGW-w64 toolchain and either `mingw32-make` or
#  build.bat (which needs no make at all). See README.md.
#
#    make            build ./DisasterManagementServer
#    make run        build and start the web server on port 8080
#    make cli        build and start the legacy console interface
#    make test       run the C++ core test suite
#    make accept     run the HTTP acceptance suite (throw-away data copy)
#    make clean      remove build artefacts
# ============================================================================
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
INCLUDES  = -Ithird_party -Isrc
LDFLAGS   = -pthread
TARGET    = DisasterManagementServer
BUILD     = build

CORE_SRC = src/csv_utils.cpp src/models.cpp src/crypto.cpp src/priority_queue.cpp \
           src/city_graph.cpp src/disaster_management.cpp src/authentication.cpp \
           src/sessions.cpp src/store.cpp src/api_json.cpp src/api_routes.cpp \
           src/input_utils.cpp src/menus.cpp src/main.cpp

HEADERS = $(wildcard src/*.h)

all: $(TARGET)

$(TARGET): $(CORE_SRC) $(HEADERS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CORE_SRC) -o $(TARGET) $(LDFLAGS)
	@echo "built: ./$(TARGET)"

$(BUILD)/test_core: tests/test_core.cpp src/csv_utils.cpp src/models.cpp src/crypto.cpp \
                    src/priority_queue.cpp src/city_graph.cpp src/disaster_management.cpp \
                    src/authentication.cpp $(HEADERS)
	@mkdir -p $(BUILD)
	$(CXX) -std=c++17 -O2 -Wall -Wextra $(INCLUDES) tests/test_core.cpp \
	  src/csv_utils.cpp src/models.cpp src/crypto.cpp src/priority_queue.cpp \
	  src/city_graph.cpp src/disaster_management.cpp src/authentication.cpp \
	  -o $(BUILD)/test_core

run: $(TARGET)
	./$(TARGET) --port 8080

cli: $(TARGET)
	./$(TARGET) --cli

check: $(TARGET)
	./$(TARGET) --check

test: $(BUILD)/test_core
	./$(BUILD)/test_core data

accept: $(TARGET)
	bash tests/acceptance.sh

clean:
	rm -rf $(BUILD) $(TARGET) $(TARGET).exe

.PHONY: all run cli check test accept clean
