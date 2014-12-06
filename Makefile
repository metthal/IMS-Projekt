PROJECT = ims-sim

SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))

CXX = g++
CXXFLAGS = -c -Wall -Wextra
CXXFLAGS_DEBUG = -g -O0
CXXFLAGS_RELEASE = -O2
LDFLAGS = -lsimlib

RM = rm -rf

TAR_FILE = 04_xmilko01_xvrabe07.tar.gz
TAR = tar -czf $(TAR_FILE)
PACKED_FILES = $(SRCS) Makefile sprava.pdf

TEX = pdflatex -interaction nonstopmode
TEX_FILE = doc/sprava.tex

#build: release
build: debug

release: CXXFLAGS += $(CXXFLAGS_RELEASE)
release: $(OBJS)
	$(CXX) $^ -o $(PROJECT) $(LDFLAGS)

debug: CXXFLAGS += $(CXXFLAGS_DEBUG)
debug: $(OBJS)
	$(CXX) $^ -o $(PROJECT) $(LDFLAGS)

run: build
# Experiment 1 - Referencne hodnoty
	./$(PROJECT) --out exp_ref.out

clean:
	$(RM) $(OBJS) $(PROJECT) sprava.*

pack: doc
	$(TAR) $(PACKED_FILES)

doc: $(TEX_FILE)
	$(TEX) $(TEX_FILE)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: build debug release run pack clean doc
