# Native Forge — portable Versailles 1685 data importer core.
# Dependency-free C++17: builds the same on Linux, Android (NDK) and Windows.
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall

vfimport: vfimport.cpp
	$(CXX) $(CXXFLAGS) -o $@ vfimport.cpp

clean:
	rm -f vfimport vfimport.exe

.PHONY: clean
