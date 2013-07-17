all: file-vtf libpixbufloader-vtf.so check
	

libpixbufloader-vtf.so: vtf.h vtf.cpp gdkpixbuf-loader-vtf.cpp
	g++ -Wall -g -shared -fPIC `pkg-config --cflags --libs gdk-pixbuf-2.0` -DGDK_PIXBUF_ENABLE_BACKEND -Ilibsquish/include -Llibsquish/lib -o libpixbufloader-vtf.so vtf.cpp gdkpixbuf-loader-vtf.cpp

file-vtf: vtf.h vtf.cpp gimp-plugin-vtf.cpp
	g++ -Wall -g -Wno-write-strings `pkg-config --cflags --libs gimp-2.0 gimpui-2.0 gtk+-2.0` -Ilibsquish/include -Llibsquish/lib -o file-vtf vtf.cpp gimp-plugin-vtf.cpp -lsquish -lboost_iostreams

check: vtf.h vtf.cpp check.c
	g++ -Wall -g `pkg-config --cflags --libs glib-2.0` -DDEBUG -Ilibsquish/include -Llibsquish/lib -o check vtf.cpp check.c

clean:
	rm -f file-vtf
	rm -f libpixbufloader-vtf.so
	rm -f check
