.DEFAULT_GOAL := help
CXX ?= g++
PYTHON ?= python3
HOST_FLAGS := -std=c++17 -O2 -g -Wall -Wextra -Werror -Wpedantic -ffp-contract=off -fno-fast-math -Iinclude
CORE := $(wildcard src/core/*.cpp)

.PHONY: help ps2 test parity reference-data registry-parity noise-parity simplex-parity float-parity factory-parity octave-parity blended-parity density-parity density-spline-parity density-data-parity check-public clean
help:
	@echo 'make ps2         -> build/ps2/MinecraftPS2.elf (PS2DEV/PS2SDK/GSKIT required)'
	@echo 'make test        -> native unit tests and import-tool tests'
	@echo 'make parity      -> compare native results with a local official 1.21.1 JAR'
	@echo 'make reference-data / registry-parity -> private state data and original-JAR transition tests'
	@echo 'make noise-parity -> exact binary64 noise sampling against original ImprovedNoise'
	@echo 'make simplex-parity -> original Simplex 2D and End island height/density'
	@echo 'make float-parity -> strict Java binary32 compatibility for the EE'
	@echo 'make factory-parity -> original positional factories, Unicode/string seeds and fork consumption'
	@echo 'make octave-parity -> original PerlinNoise/NormalNoise and all vanilla noise parameters'
	@echo 'make blended-parity -> original blended density sampling and reseeding'
	@echo 'make density-parity -> original density graph operations, bounds and lazy evaluation'
	@echo 'make density-spline-parity -> original nested float splines, bounds and lazy evaluation'
	@echo 'make density-data-parity -> private vanilla JSON graphs against independently seeded original routers'
	@echo 'make noise-chunk-parity -> original chunk caches, interpolation, counters and cell/slice order'
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

build/host/density-data: tests/density_data_runner.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/density_data_runner.cpp $(CORE) -o $@

build/host/noise-chunk: tests/noise_chunk_runner.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/noise_chunk_runner.cpp $(CORE) -o $@

build/host/noise-chunk-tests: tests/noise_chunk_tests.cpp $(CORE) $(wildcard include/mcps2/*.hpp)
	@mkdir -p $(@D)
	$(CXX) $(HOST_FLAGS) tests/noise_chunk_tests.cpp $(CORE) -o $@

.PHONY: noise-chunk-parity
noise-chunk-parity: build/host/noise-chunk
	$(PYTHON) tools/run_parity.py --runner build/host/noise-chunk --suite noise-chunk --output .local/noise-chunk-parity

test: build/host/tests build/host/registry build/host/density-data build/host/noise-chunk-tests
	./build/host/tests
	./build/host/noise-chunk-tests
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity

reference-data: build/host/registry
	$(PYTHON) tools/generate_reference_data.py --verify-runner build/host/registry

registry-parity: build/host/registry
	$(PYTHON) tools/run_parity.py --runner build/host/registry --runner-arg .local/content/block_states.bin --cases .local/content/state_cases.txt --output .local/registry-parity

noise-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite noise --output .local/noise-parity

simplex-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite simplex --output .local/simplex-parity

float-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite java-float --output .local/float-parity

factory-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite factories --output .local/factory-parity

octave-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite octaves --output .local/octave-parity

blended-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite blended --output .local/blended-parity

density-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite density --output .local/density-parity

density-spline-parity: build/host/parity
	$(PYTHON) tools/run_parity.py --runner build/host/parity --suite density-spline --output .local/density-spline-parity

density-data-parity: build/host/density-data
	$(PYTHON) tools/run_parity.py --runner build/host/density-data --suite density-data --output .local/density-data-parity

check-public:
	$(PYTHON) tools/check_public_tree.py

clean:
	rm -rf build
