CXX = g++
CXXFLAGS = -g -O2 -Wall -Wno-sign-compare -Wno-unknown-pragmas -Wno-format -D_GNU_SOURCE

mod_cul.so: module_cul.c ../../tty.c
	${CXX} $^ ${CXXFLAGS} -shared -fPIC -L. -L/opt/local/lib -I/opt/local/include -o ../../bin/$@ -lftdi
#mod_onky_av.so: module_onkyo_av.c av.c
#${CXX} $^ ${CXXFLAGS} -shared -fPIC -L. -L/share/Sources/bin -I/share/Apps/local/include -I/share/Sources/include -EL -o ../../bin/$@ -lftdi

clean: 
	rm -Rf ../../bin/*.so ../../bin/*.dSYM
