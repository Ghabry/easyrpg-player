js/xyz2png.js: xyz2png.cpp
	emcc xyz2png.cpp $(shell pkg-config --cflags --libs libpng zlib) \
		--memory-init-file 0  -s EXPORT_NAME="'xyz2png'" \
		-s MODULARIZE=1 -s EXPORTED_FUNCTIONS="['__Z5startiPPc']" \
		-s EXTRA_EXPORTED_RUNTIME_METHODS="['FS']" -Dmain='start' \
		-O3 -g0 -o js/xyz2png.js

xyz2png.cpp:
	wget -q https://raw.githubusercontent.com/EasyRPG/Tools/master/xyz2png/src/xyz2png.cpp

clean:
	rm -f xyz2png.cpp
