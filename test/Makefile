
DEBUG_FLAGS=-O0 -g -std=c++11 -I../include -I../../quickcheck/quickcheck

all: cactus_basic cactus_plus

cactus_basic: cactus-basic.cpp ../include/cactus-basic.hpp
	g++ $(DEBUG_FLAGS) cactus-basic.cpp -o cactus-basic

cactus_plus: cactus-plus.cpp ../include/cactus-plus.hpp
	g++ $(DEBUG_FLAGS) cactus-plus.cpp -o cactus-plus

clean:
	rm -f cactus-basic cactus-plus
