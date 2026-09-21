CXX ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++17

all: monitor dump replay

monitor: monitor/agxmonitor.cpp
	$(CXX) $(CXXFLAGS) monitor/agxmonitor.cpp -o agxmonitor

dump: trace/agxtracedump.cpp trace/agxtrace.hpp
	$(CXX) $(CXXFLAGS) -Itrace trace/agxtracedump.cpp -o agxtracedump

replay: trace/agxtracereplay.cpp trace/agxtrace.hpp
	$(CXX) $(CXXFLAGS) -Itrace trace/agxtracereplay.cpp -o agxtracereplay

clean:
	rm -f agxmonitor agxtracedump agxtracereplay

.PHONY: all monitor dump replay clean
