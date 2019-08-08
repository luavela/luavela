
.PHONY: all
all: lcov

lua-5.1.5.tar.gz:
	wget http://www.lua.org/ftp/lua-5.1.5.tar.gz

lua-5.2.4.tar.gz:
	wget http://www.lua.org/ftp/lua-5.2.4.tar.gz

lua-5.3.5.tar.gz:
	wget http://www.lua.org/ftp/lua-5.3.5.tar.gz

lua-5.4.0-work2.tar.gz:
	wget http://www.lua.org/work/lua-5.4.0-work2.tar.gz

.PHONY: extract
extract: lua-5.1.5.tar.gz lua-5.2.4.tar.gz lua-5.3.5.tar.gz lua-5.4.0-work2.tar.gz
	tar xvfz lua-5.1.5.tar.gz
	tar xvfz lua-5.2.4.tar.gz
	tar xvfz lua-5.3.5.tar.gz
	tar xvfz lua-5.4.0-work2.tar.gz
	git reset --hard # lua-5.*/src/Makefile

.PHONY: build
build: extract
	make -C lua-5.1.5 linux
	make -C lua-5.2.4 linux
	make -C lua-5.3.5 linux
	make -C lua-5.4.0-work2 linux

.PHONY: harness
harness: build
	-make -C ../test_lua harness LUA="../coverage/lua-5.1.5/src/lua -l profile_lua51" LUA_CPATH=";;/usr/lib/x86_64-linux-gnu/lua/5.1/?.so"
	-make -C ../test_lua harness LUA="../coverage/lua-5.2.4/src/lua -l profile_lua52" LUA_CPATH=";;/usr/lib/x86_64-linux-gnu/lua/5.2/?.so"
	-make -C ../test_lua harness LUA="../coverage/lua-5.3.5/src/lua -l profile_lua53" LUA_CPATH=";;/usr/lib/x86_64-linux-gnu/lua/5.3/?.so"
	-make -C ../test_lua harness LUA="../coverage/lua-5.4.0-work2/src/lua -l profile_lua54" LUA_CPATH=";;/usr/lib/x86_64-linux-gnu/lua/5.4/?.so"

.PHONY: lcov
lcov: harness
	make -C lua-5.1.5/src lcov
	make -C lua-5.2.4/src lcov
	make -C lua-5.3.5/src lcov
	make -C lua-5.4.0-work2/src lcov

.PHONY: clean
clean:
	rm -rf lua-5.1.5
	rm -rf lua-5.2.4
	rm -rf lua-5.3.5
	rm -rf lua-5.4.0-work2
	rm -rf ../public/cover_lua*
	git reset --hard # lua-5.*/src/Makefile

.PHONY: realclean
realclean: clean
	rm -f *.tar.gz
