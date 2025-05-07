EXE=bfjit
SRC_DIR=src

CC=g++
STD=c++23

WARNINGS=-Wsuggest-override -Wno-switch -Wno-parentheses -Wvolatile -Wextra-semi -Wimplicit-fallthrough -Wsequence-point
INCLUDES=-Ivendor/vstd -Ivendor/asmjit/src

CXXFLAGS=--std=$(STD) $(INCLUDES) -ftabstop=4 -fmax-errors=15 $(WARNINGS) -mavx -mavx2 -fno-exceptions -fconcepts-diagnostics-depth=5
LDFLAGS=-lm -lpthread -Lvendor/asmjit/build -l:libasmjit.a

.PHONY: all debug release clean run

all: debug

debug: clean
debug: CXXFLAGS+=-O0
debug: CXXFLAGS+=-g
debug: CXXFLAGS+=-DJ_DEBUG
debug: $(EXE)

release: clean
release: CXXFLAGS+=-O2
release: CXXFLAGS+=-DJ_RELEASE
release: $(EXE)

clean:
	rm -f $(EXE)

$(EXE):
	$(CC) $(CXXFLAGS) -o $(EXE) src/main.cpp $(LDFLAGS)