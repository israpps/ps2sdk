# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2009, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

# IOP_CFLAGS += -DDEBUG

IOP_IMPORT_INCS += \
	system/intrman \
	system/ioman \
	system/iomanx \
	system/loadcore \
	system/sifman \
	system/stdio \
	system/sysclib \
	system/threadman \
	tcpip/tcpip

IOP_LIBS += -lgcc

IOP_OBJS = smbman.o smb_fio.o smb.o poll.o auth.o des.o md4.o imports.o

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release
