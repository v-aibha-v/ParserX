# Builds the ParserX command line program (parserx.cpp + src/) and the
# original fixed demo (src/main.cpp), and runs the test suite.
#
#   make          build parserx and the demo
#   make test     build parserx and run every test in tests/cases
#   make clean    remove the programs and saved test output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

# Link the runtime statically on Windows so the program does not pick up
# an incompatible libstdc++ DLL from another MinGW on the PATH.
ifeq ($(OS),Windows_NT)
LDFLAGS  += -static
endif

CORE_SRCS = src/grammar.cpp src/lexer.cpp src/first_follow.cpp src/slr.cpp \
            src/clr.cpp src/lalr.cpp src/table.cpp src/parser.cpp src/comparison.cpp
HEADERS   = $(wildcard src/*.h)

.PHONY: all test clean

all: parserx parser

parserx: parserx.cpp $(CORE_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -Isrc parserx.cpp $(CORE_SRCS) -o $@ $(LDFLAGS)

parser: src/main.cpp $(CORE_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) src/main.cpp $(CORE_SRCS) -o $@ $(LDFLAGS)

test: parserx
	bash tests/run_tests.sh

clean:
	rm -f parserx parserx.exe parser parser.exe
	rm -rf tests/output
