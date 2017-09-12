python_version_full := $(wordlist 2,4,$(subst ., ,$(shell python --version 2>&1)))
python_version_major := $(word 1,${python_version_full})

ifeq ($(python_version_major), 3)
	python_interpreter := python2
else
	python_interpreter := python
endif

EMSCRIPTEN ?= /usr/lib/emscripten

all: js/xyz2png.js wildmidi/wildmidi.js

js/xyz2png.js: xyz2png.cpp
	emcc xyz2png.cpp $(shell pkg-config --cflags --libs libpng zlib) \
		--memory-init-file 0  -s EXPORT_NAME="'xyz2png'" \
		-s MODULARIZE=1 -s EXPORTED_FUNCTIONS="['__Z5startiPPc']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['FS']" -Dmain='start' \
		-O3 -g0 -o js/xyz2png.js

xyz2png.cpp:
	wget -q https://raw.githubusercontent.com/EasyRPG/Tools/master/xyz2png/src/xyz2png.cpp

wildmidi/wildmidi.js: timidity_gus
	$(python_interpreter) $(EMSCRIPTEN)/tools/file_packager.py wildmidi/wildmidi.data --preload timidity_gus@/etc --no-closure --js-output=wildmidi/wildmidi.js
	sed -i.bak "s/REMOTE_PACKAGE_BASE = 'wildmidi.data'/REMOTE_PACKAGE_BASE = 'wildmidi\/wildmidi.data'/" wildmidi/wildmidi.js

timidity_gus:
	git clone https://github.com/Ghabry/timidity_gus --depth=1
	rm -rf timidity_gus/.git

clean:
	rm -f xyz2png.cpp
	rm -rf timidity_gus
