# Makefile for the MBSPC utility
# Copyright (C) 2012 Laszlo Menczel
# Platforms: Linux and Win32/MinGW

#--------------------------------------------------------------------
# Detection of platform and architecture
#--------------------------------------------------------------------

PLATFORM = $(shell uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]')

ifeq ($(PLATFORM),linux)
  ARCH = x86_64
  TMP = $(shell uname -a | grep x86_64)
ifeq (x$(TMP),x)
    ARCH = x86
endif
endif

ifeq ($(PLATFORM),mingw32)
  ARCH = x86
endif

#--------------------------------------------------------------------
# MNT_DIR = path to MBSPC source (e.g. "/home/somebody/src/mbspc")
#--------------------------------------------------------------------

ifeq ($(PLATFORM),linux)
  MNT_DIR = $(PWD)
  BLD_DIR = $(MNT_DIR)/build/linux
  OS_CFLAGS = -D__linux__
  LIBS = -ldl -lm
  BIN_EXT =
endif

ifeq ($(PLATFORM),mingw32)
  MNT_DIR = d:/Msys/home/menczel/mbspc
  BLD_DIR = $(MNT_DIR)/build/win32
  OS_CFLAGS = -D_WIN32
  LIBS = -lws2_32 -lwinmm
  BIN_EXT = .exe
endif

#--------------------------------------------------------------------
# Locations
#--------------------------------------------------------------------

BOT_DIR = $(MNT_DIR)/botlib
BSP_DIR = $(MNT_DIR)/mbspc
COM_DIR = $(MNT_DIR)/qcommon
OBJ_DIR = $(BLD_DIR)/obj

#--------------------------------------------------------------------
# Flags
#--------------------------------------------------------------------

BASE_CFLAGS = -c -O3 -Wall -Wstrict-prototypes -fno-strict-aliasing \
              -MMD -DNDEBUG -DBSPC -DBSPCINCLUDE

CFLAGS = $(BASE_CFLAGS) $(OS_CFLAGS) -I$(MNT_DIR)/../../libs

#--------------------------------------------------------------------
# Compile commands
#--------------------------------------------------------------------

CC = gcc$(BIN_EXT)

$(OBJ_DIR)/%.o: $(BSP_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ_DIR)/%.o: $(BOT_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ_DIR)/%.o: $(COM_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

#--------------------------------------------------------------------
# Objects
#--------------------------------------------------------------------

BSP_OBJ = \
  $(OBJ_DIR)/aas_areamerging.o \
  $(OBJ_DIR)/aas_cfg.o \
  $(OBJ_DIR)/aas_create.o \
  $(OBJ_DIR)/aas_edgemelting.o \
  $(OBJ_DIR)/aas_facemerging.o \
  $(OBJ_DIR)/aas_file.o \
  $(OBJ_DIR)/aas_gsubdiv.o \
  $(OBJ_DIR)/aas_map.o \
  $(OBJ_DIR)/aas_prunenodes.o \
  $(OBJ_DIR)/aas_store.o \
  $(OBJ_DIR)/be_aas_bspc.o \
  $(OBJ_DIR)/brushbsp.o \
  $(OBJ_DIR)/l_bsp_ent.o \
  $(OBJ_DIR)/l_bsp_hl.o \
  $(OBJ_DIR)/l_bsp_q1.o \
  $(OBJ_DIR)/l_bsp_q2.o \
  $(OBJ_DIR)/l_bsp_q3.o \
  $(OBJ_DIR)/l_bsp_sin.o \
  $(OBJ_DIR)/bspc.o \
  $(OBJ_DIR)/l_cmd.o \
  $(OBJ_DIR)/csg.o \
  $(OBJ_DIR)/faces.o \
  $(OBJ_DIR)/glfile.o \
  $(OBJ_DIR)/leakfile.o \
  $(OBJ_DIR)/l_log.o \
  $(OBJ_DIR)/map.o \
  $(OBJ_DIR)/map_hl.o \
  $(OBJ_DIR)/map_q1.o \
  $(OBJ_DIR)/map_q2.o \
  $(OBJ_DIR)/map_q3.o \
  $(OBJ_DIR)/map_sin.o \
  $(OBJ_DIR)/l_math.o \
  $(OBJ_DIR)/l_mem.o \
  $(OBJ_DIR)/nodraw.o \
  $(OBJ_DIR)/l_poly.o \
  $(OBJ_DIR)/portals.o \
  $(OBJ_DIR)/prtfile.o \
  $(OBJ_DIR)/l_qfiles.o \
  $(OBJ_DIR)/textures.o \
  $(OBJ_DIR)/l_threads.o \
  $(OBJ_DIR)/tree.o \
  $(OBJ_DIR)/l_utils.o \
  $(OBJ_DIR)/writebsp.o \
  \
  $(OBJ_DIR)/be_aas_bspq3.o \
  $(OBJ_DIR)/be_aas_cluster.o \
  $(OBJ_DIR)/be_aas_move.o \
  $(OBJ_DIR)/be_aas_optimize.o \
  $(OBJ_DIR)/be_aas_reach.o \
  $(OBJ_DIR)/be_aas_sample.o \
  $(OBJ_DIR)/l_libvar.o \
  $(OBJ_DIR)/l_precomp.o \
  $(OBJ_DIR)/l_script.o \
  $(OBJ_DIR)/l_struct.o \
  \
  $(OBJ_DIR)/cm_load.o \
  $(OBJ_DIR)/cm_patch.o \
  $(OBJ_DIR)/cm_test.o \
  $(OBJ_DIR)/cm_trace.o \
  $(OBJ_DIR)/md4.o \
  $(OBJ_DIR)/unzip.o

#--------------------------------------------------------------------
# Targets
#--------------------------------------------------------------------

BSP_EXE = $(BLD_DIR)/mbspc$(BIN_EXT)

all: $(BSP_EXE)

clean:
	rm -f $(BSP_OBJ)
	rm -f $(BSP_EXE)


veryclean:
	rm -f $(BSP_OBJ)
	rm -f $(BSP_EXE)
	rm -f $(DEP_FILES)

cleandep:
	rm -f $(DEP_FILES)

#-------------------------------------------------------------------
# Build rules
#-------------------------------------------------------------------

$(BSP_EXE): $(BSP_OBJ)
	$(CC) $(BSP_LDFLAGS) -o $@ $(BSP_OBJ) $(LIBS)

#--------------------------------------------------------------------
# Dependencies
#--------------------------------------------------------------------

DEP_FILES = $(filter %.d,$(BSP_OBJ:%.o=%.d))

-include $(DEP_FILES)

.PHONY: all clean xclean cleandep
