# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

LIBPFS_PATH = $(PS2SDKSRC)/iop/hdd/libpfs/

LIBPFS_SRC_DIR = $(LIBPFS_PATH)src/

IOP_BIN ?= fssk.irx

IOP_CFLAGS += -DIOMANX_OLD_NAME_COMPATIBILITY=0 -DIOMANX_OLD_NAME_ADDDELDRV=0
IOP_CFLAGS += -DPFS_OSD_VER

IOP_IMPORT_INCS += \
	cdvd/cdvdman \
	dev9/atad \
	dev9/poweroff \
	system/intrman \
	system/iomanx \
	system/loadcore \
	system/stdio \
	system/sysclib \
	system/sysmem \
	system/threadman

IOP_INCS += -I$(LIBPFS_PATH)include

IOP_LIBS += -lgcc

PFS_OBJS = bitmap.o inode.o dir.o journal.o misc.o super.o superWrite.o cache.o block.o blockWrite.o
IOP_OBJS = fssk.o misc_fssk.o imports.o $(PFS_OBJS)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release

$(IOP_OBJS_DIR)%.o: $(LIBPFS_SRC_DIR)%.c
	$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@
