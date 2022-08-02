all: Synthesis.cpp
	g++ -std=c++23 -O3 $^ -o synthesis -lpng -lz -lpthread

debug: Synthesis.cpp
	g++ -std=c++23 -O0 -g $^ -o synthesis -lpng -lz -lpthread

optln: Synthesis.cpp
	g++ -DDBGLN -std=c++23 -O3 $^ -o synthesis -lpng -lz -lpthread

dbgln: Synthesis.cpp
	g++ -DDBGLN -std=c++23 -O0 -g $^ -o synthesis -lpng -lz -lpthread
