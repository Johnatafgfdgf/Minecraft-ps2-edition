.DEFAULT_GOAL := help
CXX ?= g++
PYTHON ?= python3
HOST_FLAGS := -std=c++17 -O2 -g -Wall -Wextra -Werror -Wpedantic -ffp-contract=off -fno-fast-math -Iinclude
CORE := $(wildcard src/core/*.cpp)

.PHONY: help ps2 test parity reference-data registry-parity noise-parity check-public clean
help:
	@echo 'make ps2         -> build/ps2/MinecraftPS2.elf (PS2DEV/PS2SDK/GSKIT required)'
	@echo 'make test        -> native unit tests and import-tool tests'
	@echo 'make parity      -> compare native results with a local official 1.21.1 JAR'
	@echo 'make reference-data / registry-parity -> private state data and original-JAR transition tests'
	@echo 'make noise-parity -> exact binary64 noise sampling against original ImprovedNoise'
	@echo 'make check-public -> reject proprietary/build inputs in tracked files'

ps2:
	$(MAKE) -f Makefile.ps2

build/host/tests: tests/core_tests.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/core_tests.cpp $(CORE) -o $@

build/host/parity: tests/parity_runner.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/parity_runner.cpp $(CORE) -o $@

build/host/registry: tests/registry_runner.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/registry_runner.cpp $(CORE) -o $@

test: build/host/tests build/host/registry
	./build/host/tests
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity

reference-data: build/host/registry
	$(PYTHON) tools/generate_reference_data.py --verify-runner build/host/registry

registry-parity: build/host/registry
	$(PYTHON) tools/run_parity.py --runner build/host/registry --runner-arg .local/content/block_states.bin --cases .local/content/state_cases.txt --output .local/registry-parity

noise-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite noise --output .local/noise-parity

check-public:
	$(PYTHON) tools/check_public_tree.py

clean:
	rm -rf build
