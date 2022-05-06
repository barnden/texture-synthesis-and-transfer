all: Synthesis.cpp
	g++ -std=c++23 $^ -o synthesis -lpng -lz

debug: Synthesis.cpp
	g++ -std=c++23 -O0 -g $^ -o synthesis -lpng -lz
