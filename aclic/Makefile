CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -g

test: aclic.cpp test_aclic.cpp aclic.hpp
	$(CXX) $(CXXFLAGS) -o test_aclic aclic.cpp test_aclic.cpp
	./test_aclic

clean:
	rm -f test_aclic

.PHONY: test clean
