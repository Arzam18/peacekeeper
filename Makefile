EXE = peacekeeper-dev
EVALFILE = src/default.nn

SOURCES := src/*.cpp

CXX ?= g++

CXXFLAGS := -std=c++17 -O3 -ffast-math -DNDEBUG -pthread \
	-DVERSION=-1 -DNETWORK_FILE=\"$(EVALFILE)\"

LINKER := -lm

# Desktop x86 builds can still use the host CPU.
ifeq ($(ANDROID),1)
CXXFLAGS += -march=armv8-a+simd
else
CXXFLAGS += -march=native
endif

OUT := $(EXE)

$(EXE): $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(OUT) $(LINKER)

clean:
	rm -f $(OUT)
