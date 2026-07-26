# Plain-make alternative to CMakeLists.txt - no cmake required, just
# g++, libX11, and libXft/fontconfig (used for the bar/launcher/
# notepad's text rendering - see include/Font.h), plus optionally
# libXrandr for multi-monitor support (auto-detected below).

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra

INCLUDES := -Iinclude
INCLUDES += $(shell pkg-config --cflags xft fontconfig)

LIBS := -lX11 -lImlib2 -lpam
LIBS += $(shell pkg-config --libs xft fontconfig)

SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC))

ifeq ($(shell pkg-config --exists xrandr && echo yes),yes)
    CXXFLAGS += -DKOHIKO_HAVE_XRANDR
    LIBS     += $(shell pkg-config --libs xrandr)
endif

.PHONY: all clean test install

all: kohiko kohikoctl

kohiko: $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LIBS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

build:
	mkdir -p build

kohikoctl: tools/kohikoctl.cpp src/IpcPath.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

test: build/test_bsptree build/test_launcherscoring
	./build/test_bsptree
	./build/test_launcherscoring

build/test_bsptree: tests/test_bsptree.cpp src/BSPTree.cpp src/BSPLeaf.cpp src/BSPSplit.cpp src/ManagedWindow.cpp src/LayoutEngine.cpp | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

# No X11/display needed either - pure string/data-structure logic,
# same reasoning as test_bsptree above - so this is folded straight
# into `make test` rather than kept separate like test-monitors is.
build/test_launcherscoring: tests/test_launcherscoring.cpp src/LauncherScoring.cpp src/Utils.cpp | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

# Needs a real X11/XRandr connection - gracefully skips the checks
# that need one if $DISPLAY isn't set (see the file itself), so it's
# kept separate from `make test` rather than folded into it.
test-monitors: build/test_monitormanager
	./build/test_monitormanager

build/test_monitormanager: tests/test_monitormanager.cpp src/Monitor.cpp src/MonitorManager.cpp src/MonitorRule.cpp src/Workspace.cpp src/WorkspaceManager.cpp src/BSPTree.cpp src/BSPLeaf.cpp src/BSPSplit.cpp src/ManagedWindow.cpp src/LayoutEngine.cpp src/Config.cpp src/XConnection.cpp src/XAtoms.cpp src/Utils.cpp src/Logger.cpp | build
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ -lX11 $(shell pkg-config --exists xrandr && pkg-config --libs xrandr)

install: kohiko kohikoctl
	install -Dm755 kohiko $(DESTDIR)/usr/local/bin/kohiko
	install -Dm755 kohikoctl $(DESTDIR)/usr/local/bin/kohikoctl
	install -Dm644 config/default.conf $(DESTDIR)/usr/local/share/kohiko/default.conf

clean:
	rm -rf build kohiko kohikoctl
