BUILD_DIR = build

.PHONY: build

sync_libev:
	rm -rf libs/libev
	cd libs && cvs -z3 -d :pserver:anonymous@cvs.schmorp.de/schmorpforge co libev

apply_libev_patch:
	ls -d libs/patches/libev/* | xargs -I{} sh -c "patch -s -p0 <{}"

install_libev: sync_libev apply_libev_patch
	cd libs/libev && sh ./autogen.sh && ./configure && make -j$(nproc) && make install && make distclean

build: clean
	mkdir -p ${BUILD_DIR}/bin
	$(CXX) -O3 -std=c++17 src/main.cpp -l:libev.a -l:librt.a -o ${BUILD_DIR}/bin/test

clean:
	rm -rf $(BUILD_DIR)

