all: file-vtf libpixbufloader-vtf.so check
	

libpixbufloader-vtf.so: dxt.h dxt.c vtf.h vtf.c gdkpixbuf-loader-vtf.c
	gcc -Wall -g -shared -fPIC `pkg-config --cflags --libs gdk-pixbuf-2.0` -DGDK_PIXBUF_ENABLE_BACKEND -o libpixbufloader-vtf.so vtf.c gdkpixbuf-loader-vtf.c

file-vtf: vtf.h vtf.cpp gimp-plugin-vtf.cpp
	g++ -Wall -g `pkg-config --cflags --libs gimp-2.0 gimpui-2.0 gtk+-2.0` -Ilibsquish/include -Llibsquish/lib -lsquish -o file-vtf vtf.cpp gimp-plugin-vtf.cpp

check: dxt.h dxt.c vtf.h vtf.c check.c
	gcc -Wall -g `pkg-config --cflags --libs glib-2.0` -DDEBUG -o check vtf.c check.c

clean:
	rm file-vtf
	rm libpixbufloader-vtf.so
	rm check
