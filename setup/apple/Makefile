# Makefile for NetRadiant.app, requires http://macdylibbundler.sourceforge.net/

INSTALL = ../../install
TARGET = target
RESOURCES = $(TARGET)/NetRadiant.app/Contents/Resources
BINDIR = $(RESOURCES)/install
CONFDIR = $(RESOURCES)/etc
DATADIR = $(RESOURCES)/share
LIBDIR = $(RESOURCES)/lib
VERSION = 1.5.0
DMG = $(TARGET)/NetRadiant-$(VERSION).dmg
VOLUME_NAME = "NetRadiant $(VERSION)"

# We must rewrite some Gtk configuration files, which vary from environment to
# environment depending on where you've installed MacPorts. Fun!

MACPORTS_PREFIX := $(shell which port | sed 's:/bin/port::')
PREFIX_SED_EXPR = "s:$(MACPORTS_PREFIX):@executable_path/..:g"

all: install bundle

-pre-install:
	install -d $(TARGET)
	cp -r NetRadiant.app $(TARGET)
	find $(TARGET) -name .turd -delete
	
-gtk-runtime-gdk-pixbuf-2.0:
	cp -r $(MACPORTS_PREFIX)/lib/gdk-pixbuf-2.0 $(LIBDIR)
	find $(LIBDIR)/gdk-pixbuf-2.0 -type f ! -name "*.so" -delete
	
	gdk-pixbuf-query-loaders | sed $(PREFIX_SED_EXPR) > \
		$(CONFDIR)/gtk-2.0/gdk-pixbuf.loaders

-gtk-runtime-pango:
	#cp -r $(MACPORTS_PREFIX)/lib/pango $(LIBDIR)
	#find $(LIBDIR)/pango -type f ! -name "*.so" -delete
	
	#pango-querymodules | sed $(PREFIX_SED_EXPR) > \
		$(CONFDIR)/pango/pango.modules
	
-gtk-runtime: -gtk-runtime-gdk-pixbuf-2.0 -gtk-runtime-pango
	cp -r $(MACPORTS_PREFIX)/lib/gtk-2.0 $(LIBDIR)
	find $(LIBDIR)/gtk-2.0 -type f ! -name "*.so" -delete	
	
	rm -rf $(LIBDIR)/gtk-2.0/{includes,modules}
	rm -rf $(LIBDIR)/gtk-2.0/*/printbackends
	
	cp -r $(MACPORTS_PREFIX)/share/themes/Default $(RESOURCES)/share
	
	gtk-query-immodules-2.0 | sed $(PREFIX_SED_EXPR) > \
		$(CONFDIR)/gtk-2.0/gtk.immodules

install: -pre-install -gtk-runtime
	cp -r $(INSTALL) $(RESOURCES)
	#rm -rf `find $(INSTDIR)/installs -type d -name .svn`

bundle:
	
	# The Radiant plugins (modules) are a little funky
	# Some of them are actually linked against the build directory
	
	test -L install || ln -s $(INSTALL)
	
	dylibbundler -b \
		-x $(BINDIR)/radiant.x86_64 \
		-x $(BINDIR)/q2map.x86_64 \
		-x $(BINDIR)/q3data.x86_64 \
		-x $(BINDIR)/q3map2.x86_64 \
		-x $(BINDIR)/qdata3.x86_64 \
		`find $(BINDIR)/modules -name "*.dylib" | xargs -I {} echo -x {}` \
		`find $(LIBDIR) -name "*.dylib" | xargs -I {} echo -x {}` \
	-d $(LIBDIR) -of -p @executable_path/../lib
	
	find ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders -name "*.so" -exec install_name_tool -change /opt/local/lib/libgdk_pixbuf-2.0.0.dylib @executable_path/../lib/libgdk_pixbuf-2.0.0.dylib {} \;
	find ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders -name "*.so" -exec install_name_tool -change /opt/local/lib/libglib-2.0.0.dylib @executable_path/../lib/libglib-2.0.0.dylib {} \;
	find ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders -name "*.so" -exec install_name_tool -change /opt/local/lib/libgobject-2.0.0.dylib @executable_path/../lib/libgobject-2.0.0.dylib {} \;

	install_name_tool -change /opt/local/lib/libgio-2.0.0.dylib @executable_path/../lib/libgio-2.0.0.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-ico.so
	install_name_tool -change /opt/local/lib/libjasper.4.dylib @executable_path/../lib/libjasper.4.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jasper.so
	install_name_tool -change /opt/local/lib/libjpeg.9.dylib @executable_path/../lib/libjpeg.9.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jpeg.so
	install_name_tool -change /opt/local/lib/libpng16.16.dylib @executable_path/../lib/libpng16.16.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.so
	install_name_tool -change /opt/local/lib/libintl.8.dylib @executable_path/../lib/libintl.8.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-qtif.so
	install_name_tool -change /opt/local/lib/libtiff.5.dylib @executable_path/../lib/libtiff.5.dylib ${LIBDIR}/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-tiff.so

	rm -f install

	chmod +x $(TARGET)/NetRadiant.app/Contents/MacOS/radiant

image:
	find $(TARGET) -name .DS_Store -delete
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create -ov $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME)
	rm $(TARGET)/Applications

clean:
	rm -rf $(TARGET)/*
