CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic
SOURCES := src/main.cpp src/lexer.cpp src/parser.cpp src/chess.cpp src/compiler.cpp src/report.cpp src/json.cpp

.PHONY: all test serve clean
all: build/chessscript

build/chessscript: $(SOURCES) include/chessscript.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude $(SOURCES) -o $@

build/chess_rules: tests/chess_rules.cpp src/chess.cpp src/lexer.cpp include/chessscript.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude tests/chess_rules.cpp src/chess.cpp src/lexer.cpp -o $@

test: build/chessscript build/chess_rules
	sh tests/smoke.sh ./build/chessscript
	./build/chess_rules

serve: build/chessscript
	python3 web/server.py

clean:
	rm -rf build
