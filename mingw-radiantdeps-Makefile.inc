# where the dependencies package is extracted
RADIANT_DEPENDENCIES_PREFIX = $(HOME)/netradiant-dependencies-mingw32/

# run the prefix fixing tool
IGNORETHIS := $(shell cd $(RADIANT_DEPENDENCIES_PREFIX) && ./fixprefix.sh)

# set the environment for make
CFLAGS +=
CXXFLAGS +=
CPPFLAGS += -I$(RADIANT_DEPENDENCIES_PREFIX)/radiantdeps/include
LDFLAGS += -L$(RADIANT_DEPENDENCIES_PREFIX)/radiantdeps/lib
CPPFLAGS += -I$(RADIANT_DEPENDENCIES_PREFIX)/gtk/include
LDFLAGS += -L$(RADIANT_DEPENDENCIES_PREFIX)/gtk/lib
PKG_CONFIG_PATH := $(RADIANT_DEPENDENCIES_PREFIX)/gtk/lib/pkgconfig:$(RADIANT_DEPENDENCIES_PREFIX)/radiantdeps/lib/pkgconfig:$(PKG_CONFIG_PATH)
WHICHDLL = $(RADIANT_DEPENDENCIES_PREFIX)/whichdll.sh
