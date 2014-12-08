PROJECT = ims-sim

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))

CXX = g++
CXXFLAGS = -c -Wall -Wextra
CXXFLAGS_DEBUG = -g -O0
CXXFLAGS_RELEASE = -O2
LDFLAGS = -lsimlib

RM = rm -rf

TAR_FILE = 04_xmilko01_xvrabe07.tar.gz
TAR = tar -czf $(TAR_FILE)
PACKED_FILES = $(SRCS) $(HDRS) Makefile sprava.pdf

TEX = pdflatex -interaction nonstopmode
TEX_FILE = doc/sprava.tex

build: release
#build: debug

release: CXXFLAGS += $(CXXFLAGS_RELEASE)
release: $(OBJS)
	$(CXX) $^ -o $(PROJECT) $(LDFLAGS)

debug: CXXFLAGS += $(CXXFLAGS_DEBUG)
debug: $(OBJS)
	$(CXX) $^ -o $(PROJECT) $(LDFLAGS)

run: build
	@echo ""
	@echo "Experiment 1 - Referencne hodnoty"
	./$(PROJECT) --out exp_ref.out
	@echo ""
	@echo "Experiment 2 - Zrychlenie P&P zariadeni"
	./$(PROJECT) --out exp_pnp.out --pnp 25 40
	@echo ""
	@echo "Experiment 3 - Zvysena produkcia"
	./$(PROJECT) --out exp_pro.out --req 35000 --smt 25 --dip 3 --tst 6 --pkg 3
	@echo ""
	@echo "Experiment 4 - Zvysena chybovost"
	./$(PROJECT) --out exp_err.out --err 30 30

clean:
	$(RM) $(OBJS) $(PROJECT) sprava.*

pack: doc
	$(TAR) $(PACKED_FILES)

doc: $(TEX_FILE)
	$(TEX) $(TEX_FILE)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: build debug release run pack clean doc
