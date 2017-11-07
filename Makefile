#/* ===================================================
# * Copyright (C) 2017 hangzhou nanyuntech All Right Reserved.
# *      Author: (chenshuangping)mincore@163.com
# *    Filename: Makefile
# *     Created: 2017-05-31 13:48
# * Description: 
# * ===================================================
# */
CXX=@echo "cc $@";g++

DEPLIBDIR=dep/lib
DEPINCDIR=dep/include
BINDIR=bin
TARGET=$(BINDIR)/libfc.so
TEST=$(BINDIR)/test

CXXFLAGS=-g -Wall -fPIC
CXXFLAGS+=-I$(DEPINCDIR) -Isrc -Iinclude
CXXFLAGS+=-I/usr/local/cuda/include
#CXXFLAGS+=-DCOST_PROFILING

OBJS=$(patsubst %.cpp,%.o,$(shell find src -name *.cpp))
TEST_OBJS=$(patsubst %.cpp,%.o,$(shell find test -name *.cpp))

LDFLAGS+=-Wl,-rpath=.
DEPS=\
	 -lpthread\
	 -lleveldb\
	 -lprotobuf\
	 -L/usr/local/cuda/lib64\
	 -lcudart

DEPS+=`pkg-config  --cflags --libs opencv`

ifeq ($(USE_FAKE_ALG), 1)
	CXXFLAGS+=-DUSE_FAKE_ALG
else
DEPS+=-Lbin -lalga -lcaffe -lindex
endif

#ifeq ($(USE_NUMA), 0)
#else
#	CXXFLAGS+=-DUSE_NUMA
#	DEPS+=-lnuma
#endif

LDFLAGS+=$(DEPS)

all: $(TARGET) test

$(TARGET): $(OBJS)
	$(CXX) $^ -shared -fPIC -o $@ $(LDFLAGS)

$(TEST): $(TEST_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS) -Lbin -lfc

%.o:%.cpp
	$(CXX) $< -c -o $@ $(CXXFLAGS) -std=c++11

.PHONY : test clean

test: $(TARGET) $(TEST)

clean:
	@rm -f $(OBJS) $(TARGET) $(TEST) $(TEST_OBJS)
