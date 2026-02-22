
all: jai

CXX ?= c++
CXXFLAGS ?= -std=gnu++23 -Wall -Werror -ggdb
#CPPFLAGS += $(shell pkg-config --cflags mount)
#LDLIBS += $(shell pkg-config --libs mount)

jai: jai.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f jai *~ *.o

.PHONY: clean
