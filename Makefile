
all: jai

CXX ?= c++
CXXFLAGS ?= -std=gnu++23 -Wall -Werror -ggdb
#CPPFLAGS += $(shell pkg-config --cflags mount)
#LDLIBS += $(shell pkg-config --libs mount)

jai: jai.cc jai.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f jai *~ *.o

.PHONY: clean
