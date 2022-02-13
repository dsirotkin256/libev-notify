BUILD_DIR = build

.PHONY: build

sync_libev:
	rm -rf libev
	cvs -z3 -d :pserver:anonymous@cvs.schmorp.de/schmorpforge co libev

install_libev:
	cd libev && sh ./autogen.sh && ./configure && make -j$(nproc) && make install

gen_git_libev: sync_libev install_libev
	cd libev &&  find . ! -path . -type l,d -printf '%P\n' > .gitignore
	$(MAKE) clean_libev

build: clean
	mkdir -p ${BUILD_DIR}
	$(CXX) -O3 -std=c++17 src/main.cpp -l:libev.a -o build/test

clean:
	rm -rf $(BUILD_DIR)

clean_libev:
	cd libev && make distclean

