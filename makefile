CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
CPPFLAGS += -Iinclude
SFML_LIBS = -lsfml-graphics -lsfml-window -lsfml-system
HEADERS = $(wildcard include/ants/*.hpp)

all: ants

ants: src/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< $(SFML_LIBS)

# no SFML needed
headless: src/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -DANTS_NO_SFML -o ants-headless $<

ants_tests: tests/test_ants.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

test: ants_tests
	./ants_tests

clean:
	rm -f ants ants-headless ants_tests

.PHONY: all headless test clean
