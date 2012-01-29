CXX = g++
CXXFLAGS = -g -O2 -Wall -Wno-sign-compare -Wno-unknown-pragmas -Wno-format -D_GNU_SOURCE
mod_onkyo_av.so : module_onkyo_av.c av.c
	${CXX} $^ ${CXXFLAGS} -shared -fPIC -L. -o ../../bin/$@

clean: 
	rm -Rf ../../bin/*.so ../../bin/*.dSYM
