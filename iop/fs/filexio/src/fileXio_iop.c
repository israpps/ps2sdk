/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * fileXio RPC Server
 * This module provides an RPC interface to the EE for all the functions
 * of ioman/fileio.
 */

//#define DEBUG

#include <types.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <intrman.h>
#include <iomanX.h>
#include <loadcore.h>
#include <sysmem.h>
#include <sifman.h>
#include <sifcmd.h>
#include <errno.h>
#include <fileXio.h>

#define MODNAME "fileXio"
IRX_ID("IOX/File_Manager_Rpc", 1, 2);

#define M_PRINTF(format, args...) \
    printf(MODNAME ": " format, ##args)

#ifdef DEBUG
#define M_DEBUG M_PRINTF
#else
#define M_DEBUG(format, args...)
#endif

#define TRUE	1
#define FALSE	0

#define MIN(a, b)	(((a)<(b))?(a):(b))
#define RDOWN_64(a)	((unsigned int)(a)&~0x3F)

#define DEFAULT_RWSIZE	16384
static void *rwbuf = NULL;
static unsigned int RWBufferSize=DEFAULT_RWSIZE;

// 0x4800 bytes for DirEntry structures
// 0x400 bytes for the filename string
static unsigned char fileXio_rpc_buffer[0x4C00] __attribute__((__aligned__(4))); // RPC send/receive buffer
struct t_SifRpcDataQueue qd;
struct t_SifRpcServerData sd0;

static rests_pkt rests;

/* RPC exported functions */
static int fileXio_GetDeviceList_RPC(struct fileXioDevice* ee_devices, int eecount);
static int fileXio_CopyFile_RPC(const char *src, const char *dest, int mode);
static int fileXio_Read_RPC(int infd, char *read_buf, int read_size, void *intr_data);
static int fileXio_Write_RPC(int outfd, const char *write_buf, int write_size, int mis,u8 *misbuf);
static int fileXio_GetDir_RPC(const char* pathname, struct fileXioDirEntry dirEntry[], unsigned int req_entries);
static int fileXio_Mount_RPC(const char* mountstring, const char* mountpoint, int flag);
static int fileXio_chstat_RPC(char *filename, void* eeptr, int mask);
static int fileXio_getstat_RPC(char *filename, void* eeptr);
static int fileXio_dread_RPC(int fd, void* eeptr);

// Functions called by the RPC server
static void* fileXioRpc_Stop();
static void* fileXioRpc_GetDeviceList(unsigned int* sbuff);
static void* fileXioRpc_Getdir(unsigned int* sbuff);
static void* fileXioRpc_Mount(unsigned int* sbuff);
static void* fileXioRpc_uMount(unsigned int* sbuff);
static void* fileXioRpc_CopyFile(unsigned int* sbuff);
static void* fileXioRpc_MkDir(unsigned int* sbuff);
static void* fileXioRpc_RmDir(unsigned int* sbuff);
static void* fileXioRpc_Remove(unsigned int* sbuff);
static void* fileXioRpc_Rename(unsigned int* sbuff);
static void* fileXioRpc_SymLink(unsigned int* sbuff);
static void* fileXioRpc_ReadLink(unsigned int* sbuff);
static void* fileXioRpc_ChDir(unsigned int* sbuff);
static void* fileXioRpc_Open(unsigned int* sbuff);
static void* fileXioRpc_Close(unsigned int* sbuff);
static void* fileXioRpc_Read(unsigned int* sbuff);
static void* fileXioRpc_Write(unsigned int* sbuff);
static void* fileXioRpc_Lseek(unsigned int* sbuff);
static void* fileXioRpc_Lseek64(unsigned int* sbuff);
static void* fileXioRpc_ChStat(unsigned int* sbuff);
static void* fileXioRpc_GetStat(unsigned int* sbuff);
static void* fileXioRpc_Format(unsigned int* sbuff);
static void* fileXioRpc_AddDrv(unsigned int* sbuff);
static void* fileXioRpc_DelDrv(unsigned int* sbuff);
static void* fileXioRpc_Sync(unsigned int* sbuff);
static void* fileXioRpc_Devctl(unsigned int* sbuff);
static void* fileXioRpc_Ioctl(unsigned int* sbuff);
static void* fileXioRpc_Ioctl2(unsigned int* sbuff);
static void* fileXioRpc_Dopen(unsigned int* sbuff);
static void* fileXioRpc_Dread(unsigned int* sbuff);
static void* fileXioRpc_Dclose(unsigned int* sbuff);
static void* filexioRpc_SetRWBufferSize(void *sbuff);
static void* fileXioRpc_Getdir(unsigned int* sbuff);
static void DirEntryCopy(struct fileXioDirEntry* dirEntry, iox_dirent_t* internalDirEntry);

// RPC server
static void* fileXio_rpc_server(int fno, void *data, int size);
static void fileXio_Thread(void* param);

int _start( int argc, char *argv[])
{
	struct _iop_thread param;
	int th, result;

	(void)argc;
	(void)argv;

	param.attr         = TH_C;
	param.thread       = (void*)fileXio_Thread;
	param.priority 	  = 40;
	param.stacksize    = 0x8000;
	param.option      = 0;

	th = CreateThread(&param);

	if (th > 0)
	{
		StartThread(th, NULL);
		result=MODULE_RESIDENT_END;
	}
	else result=MODULE_NO_RESIDENT_END;

	return result;
}

static int fileXio_GetDeviceList_RPC(struct fileXioDevice* ee_devices, int eecount)
{
    int device_count = 0;
    iomanX_iop_device_t **devices = iomanX_GetDeviceList();
    struct fileXioDevice local_devices[FILEXIO_MAX_DEVICES];
    while (device_count < eecount && devices[device_count])
    {
        iomanX_iop_device_t *device = devices[device_count];
        strncpy(local_devices[device_count].name, device->name, 128);
        local_devices[device_count].name[15] = '\0';
        local_devices[device_count].type = device->type;
        local_devices[device_count].version = device->version;
        strncpy(local_devices[device_count].desc, device->desc, 128);
        local_devices[device_count].desc[127] = '\0';
        ++device_count;
    }
    if (device_count)
    {
        struct t_SifDmaTransfer dmaStruct;
        int intStatus;	// interrupt status - for dis/en-abling interrupts

        dmaStruct.src = local_devices;
        dmaStruct.dest = ee_devices;
        dmaStruct.size = sizeof(struct fileXioDevice) * device_count;
        dmaStruct.attr = 0;

        // Do the DMA transfer
        CpuSuspendIntr(&intStatus);
        sceSifSetDma(&dmaStruct, 1);
        CpuResumeIntr(intStatus);
    }
    return device_count;
}

static int fileXio_CopyFile_RPC(const char *src, const char *dest, int mode)
{
  iox_stat_t stat;
  int infd, outfd, size, remain, i;

  if ((infd = iomanX_open(src, FIO_O_RDONLY, 0666)) < 0) {
    return infd;
  }
  if ((outfd = iomanX_open(dest, FIO_O_RDWR|FIO_O_CREAT|FIO_O_TRUNC, 0666)) < 0) {
    iomanX_close(infd);
    return outfd;
  }
  size = iomanX_lseek(infd, 0, FIO_SEEK_END);
  iomanX_lseek(infd, 0, FIO_SEEK_SET);
  if (!size)
    return 0;

  remain = size % RWBufferSize;
  for (i = 0; (unsigned int)i < (size / RWBufferSize); i++) {
    iomanX_read(infd, rwbuf, RWBufferSize);
    iomanX_write(outfd, rwbuf, RWBufferSize);
  }
  iomanX_read(infd, rwbuf, remain);
  iomanX_write(outfd, rwbuf, remain);
  iomanX_close(infd);
  iomanX_close(outfd);

  stat.mode = mode;
  iomanX_chstat(dest, &stat, 1);

  return size;
}

static int fileXio_Read_RPC(int infd, char *read_buf, int read_size, void *intr_data)
{
     int srest;
     int erest;
     int asize;
     int abuffer;
     int rlen;
     int total;
     int readlen;
     int status;
     struct t_SifDmaTransfer dmaStruct;
     void *buffer;
     void *aebuffer;
     int intStatus;	// interrupt status - for dis/en-abling interrupts
     int read_buf2 = (int)read_buf;

	total = 0;

	if(read_size < 64)
	{
		srest = read_size;
		erest = asize = abuffer = 0;
		buffer = read_buf;
		aebuffer = 0;
	}else{
		if ((read_buf2 & 0x3F) == 0)
			srest=0;
		else
			srest=(int)RDOWN_64(read_buf2) - read_buf2 + 64;
		buffer = read_buf;
		abuffer = read_buf2 + srest;
		aebuffer=(void *)RDOWN_64(read_buf2 + read_size);
		asize = (int)aebuffer - (int)abuffer;
		erest = (read_buf2 + read_size) - (int)aebuffer;
	}
	if (srest>0)
	{
		if (srest!=(rlen=iomanX_read(infd, rests.sbuffer, srest)))
		{
			total += srest = (rlen>0 ? rlen:0);
			goto EXIT;
		}
		else
			total += srest;
	}

	status=0;
	while (asize>0)
	{
		readlen=MIN(RWBufferSize, (unsigned int)asize);

		while(sceSifDmaStat(status)>=0);

		rlen=iomanX_read(infd, rwbuf, readlen);
		if (readlen!=rlen){
			if (rlen<=0)goto EXIT;
			dmaStruct.dest=(void *)abuffer;
			dmaStruct.size=rlen;
			dmaStruct.attr=0;
			dmaStruct.src =rwbuf;
			CpuSuspendIntr(&intStatus);
			sceSifSetDma(&dmaStruct, 1);
			CpuResumeIntr(intStatus);
			total	+=rlen;
			goto EXIT;
		}else{			//read ok
			total   += rlen;
			asize   -=rlen;
			dmaStruct.dest=(void *)abuffer;
			abuffer +=rlen;
			dmaStruct.size=rlen;
			dmaStruct.attr=0;
			dmaStruct.src =rwbuf;
			CpuSuspendIntr(&intStatus);
			status=sceSifSetDma(&dmaStruct, 1);
			CpuResumeIntr(intStatus);
		}
	}
	if (erest>0)
	{
		rlen = iomanX_read(infd, rests.ebuffer, erest);
		total += (rlen>0 ? rlen : 0);
	}
EXIT:
	rests.ssize=srest;
	rests.esize=erest;
	rests.sbuf =buffer;
	rests.ebuf =aebuffer;
      dmaStruct.src =&rests;
	dmaStruct.size=sizeof(rests_pkt);
	dmaStruct.attr=0;
	dmaStruct.dest=intr_data;
	CpuSuspendIntr(&intStatus);
	sceSifSetDma(&dmaStruct, 1);
	CpuResumeIntr(intStatus);
	return (total);
}

static int fileXio_Write_RPC(int outfd, const char *write_buf, int write_size, int mis,u8 *misbuf)
{
     SifRpcReceiveData_t rdata;
     int left;
     int wlen;
     int pos;
     int total;

	left  = write_size;
	total = 0;
	if (mis > 0)
      {
		wlen=iomanX_write(outfd, misbuf, mis);
		if (wlen != mis)
            {
			if (wlen > 0)
				total += wlen;
			return (total);
		}
		total += wlen;
	}

	left-=mis;
	pos=(int)write_buf+mis;
	while(left){
		int writelen;
		writelen = MIN(RWBufferSize, (unsigned int)left);
		sceSifGetOtherData(&rdata, (void *)pos, rwbuf, writelen, 0);
		wlen=iomanX_write(outfd, rwbuf, writelen);
		if (wlen != writelen){
			if (wlen>0)
				total+=wlen;
			return (total);
		}
		left -=writelen;
		pos  +=writelen;
		total+=writelen;
	}
	return (total);
}


// This is the getdir for use by the EE RPC client
// It DMA's entries to the specified buffer in EE memory
static int fileXio_GetDir_RPC(const char* pathname, struct fileXioDirEntry dirEntry[], unsigned int req_entries)
{
	int matched_entries;
      int fd;
  	iox_dirent_t dirbuf;
	struct fileXioDirEntry localDirEntry;
	int intStatus;	// interrupt status - for dis/en-abling interrupts
	struct t_SifDmaTransfer dmaStruct;

	M_DEBUG("GetDir Request '%s'\n",pathname);

	matched_entries = 0;

      fd = iomanX_dopen(pathname);
      if (fd <= 0)
      {
        return fd;
      }
	{
		int res;

        res = 1;
        while (res > 0)
        {
          memset(&dirbuf, 0, sizeof(dirbuf));
          res = iomanX_dread(fd, &dirbuf);
          if (res > 0)
          {
		  int dmaID;

		  dmaID = 0;

		// check for too many entries
		if ((unsigned int)matched_entries == req_entries)
		{
			iomanX_close(fd);
			return (matched_entries);
		}
		// wait for any previous DMA to complete
	      // before over-writing localDirEntry
	      while(sceSifDmaStat(dmaID)>=0);
            DirEntryCopy(&localDirEntry, &dirbuf);
	      // DMA localDirEntry to the address specified by dirEntry[matched_entries]
	      // setup the dma struct
	      dmaStruct.src = &localDirEntry;
	      dmaStruct.dest = &dirEntry[matched_entries];
	      dmaStruct.size = sizeof(struct fileXioDirEntry);
	      dmaStruct.attr = 0;
	      // Do the DMA transfer
	      CpuSuspendIntr(&intStatus);
	      dmaID = sceSifSetDma(&dmaStruct, 1);
	      CpuResumeIntr(intStatus);
  	      matched_entries++;
          } // if res
        } // while

      } // if dirs and files

      // cleanup and return # of entries
      iomanX_close(fd);
	return (matched_entries);
}

// This is the mount for use by the EE RPC client
static int fileXio_Mount_RPC(const char* mountstring, const char* mountpoint, int flag)
{
	int res=0;
	M_DEBUG("Mount Request mountpoint:'%s' - '%s' - %d\n",mountpoint,mountstring,flag);
	res = iomanX_mount(mountpoint, mountstring, flag, NULL, 0);
	return(res);
}

static int fileXio_chstat_RPC(char *filename, void* eeptr, int mask)
{
      int res=0;
	iox_stat_t localStat;
      SifRpcReceiveData_t rdata;

	sceSifGetOtherData(&rdata, (void *)eeptr, &localStat, 64, 0);

	res = iomanX_chstat(filename, &localStat, mask);
      return(res);
}

static int fileXio_getstat_RPC(char *filename, void* eeptr)
{
	iox_stat_t localStat;
	int res = 0;
	struct t_SifDmaTransfer dmaStruct;
	int intStatus;	// interrupt status - for dis/en-abling interrupts

	res = iomanX_getstat(filename, &localStat);

	if(res >= 0)
	{
		// DMA localStat to the address specified by eeptr
		// setup the dma struct
		dmaStruct.src = &localStat;
		dmaStruct.dest = eeptr;
		dmaStruct.size = sizeof(iox_stat_t);
		dmaStruct.attr = 0;
		// Do the DMA transfer
		CpuSuspendIntr(&intStatus);
		sceSifSetDma(&dmaStruct, 1);
		CpuResumeIntr(intStatus);
	}

	return(res);
}

static int fileXio_dread_RPC(int fd, void* eeptr)
{
      int res=0;
	iox_dirent_t localDir;
      struct t_SifDmaTransfer dmaStruct;
      int intStatus;	// interrupt status - for dis/en-abling interrupts

	res = iomanX_dread(fd, &localDir);
      if (res > 0)
      {
	  // DMA localStat to the address specified by eeptr
	  // setup the dma struct
	  dmaStruct.src = &localDir;
	  dmaStruct.dest = eeptr;
	  dmaStruct.size = 64+256;
	  dmaStruct.attr = 0;
	  // Do the DMA transfer
	  CpuSuspendIntr(&intStatus);
	  sceSifSetDma(&dmaStruct, 1);
	  CpuResumeIntr(intStatus);
      }

      return(res);
}

static void* fileXioRpc_Stop(void)
{
	M_DEBUG("Stop Request\n");
	return NULL;
}

// Send:   Offset 0 = pointer to array of fileXioDevice structures in EE mem
// Send:   Offset 4 = requested number of entries
// Return: Offset 0 = ret val (number of entries). Size = int
static void* fileXioRpc_GetDeviceList(unsigned int* sbuff)
{
	int ret;
	struct fxio_devlist_packet *packet=(struct fxio_devlist_packet*)sbuff;

	ret=fileXio_GetDeviceList_RPC(
		packet->deviceEntry,
		packet->reqEntries
		);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filename string (512 bytes)
// Send:   Offset 512 = pointer to array of DirEntry structures in EE mem
// Send:   Offset 516 = requested number of entries
// Return: Offset 0 = ret val (number of matched entries). Size = int
static void* fileXioRpc_Getdir(unsigned int* sbuff)
{
	int ret;
	struct fxio_getdir_packet *packet=(struct fxio_getdir_packet*)sbuff;

	ret=fileXio_GetDir_RPC(
		packet->pathname,
		packet->dirEntry,
		packet->reqEntries
		);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = mount format string (512 bytes)
// Send:   Offset 512 = mount point (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Mount(unsigned int* sbuff)
{
	struct fxio_mount_packet *packet=(struct fxio_mount_packet*)sbuff;
	int ret;

	M_DEBUG("Mount Request\n");
	ret=fileXio_Mount_RPC(
		packet->blockdevice,		// mount format string
		packet->mountpoint,		// mount point
		packet->flags			// flag (normal,readonly,robust)
		);

	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = mount format string (512 bytes)
// Send:   Offset 512 = mount point (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_uMount(unsigned int* sbuff)
{
	int ret;
	struct fxio_unmount_packet *packet=(struct fxio_unmount_packet*)sbuff;

	M_DEBUG("uMount Request\n");
	ret=iomanX_umount(packet->mountpoint);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = source filename (512 bytes)
// Send:   Offset 512 = destination filename (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_CopyFile(unsigned int* sbuff)
{
	int ret;
	struct fxio_copyfile_packet *packet=(struct fxio_copyfile_packet*)sbuff;

	ret=fileXio_CopyFile_RPC(
		packet->source,		// source filename
		packet->dest,		// dest filename
		packet->mode		// mode
		);

	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filenames (512 bytes)
// Send:   Offset 512 = mode (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_MkDir(unsigned int* sbuff)
{
	int ret;
	struct fxio_mkdir_packet *packet=(struct fxio_mkdir_packet*)sbuff;

	M_DEBUG("MkDir Request\n");
	ret=iomanX_mkdir(packet->pathname, packet->mode);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filenames (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_RmDir(unsigned int* sbuff)
{
	int ret;
	struct fxio_pathsel_packet *packet=(struct fxio_pathsel_packet*)sbuff;

	M_DEBUG("RmDir Request\n");
	ret=iomanX_rmdir(packet->pathname);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filenames (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Remove(unsigned int* sbuff)
{
	int ret;
	struct fxio_pathsel_packet *packet=(struct fxio_pathsel_packet*)sbuff;

	M_DEBUG("Remove Request\n");
	ret=iomanX_remove(packet->pathname);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = source filename (512 bytes)
// Send:   Offset 512 = destination filename (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Rename(unsigned int* sbuff)
{
	int ret;
	struct fxio_rename_packet *packet=(struct fxio_rename_packet*)sbuff;

	M_DEBUG("Rename Request\n");
	ret=iomanX_rename(packet->source, packet->dest);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = source filename (512 bytes)
// Send:   Offset 512 = destination filename (512 bytes)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_SymLink(unsigned int* sbuff)
{
	int ret;
	struct fxio_rename_packet *packet=(struct fxio_rename_packet*)sbuff;

	M_DEBUG("SymLink Request\n");
	ret=iomanX_symlink(packet->source, packet->dest);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = source filename (512 bytes)
// Send:   Offset 512 = buffer (512 bytes)
// Send:   Offset 1024 = buflen (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_ReadLink(unsigned int* sbuff)
{
	int ret;
	struct fxio_readlink_packet *packet=(struct fxio_readlink_packet*)sbuff;

	M_DEBUG("ReadLink Request\n");
	ret=iomanX_readlink(packet->source, packet->buffer, packet->buflen);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filename (512 bytes)
// Return: Offset 0 = return status (int)
void* fileXioRpc_ChDir(unsigned int* sbuff)
{
	int ret;
	M_DEBUG("ChDir Request '%s'\n", (char*)sbuff);
	ret=iomanX_chdir((char*)sbuff);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = source filename (512 bytes)
// Send:   Offset 512 = flags (int)
// Send:   Offset 516 = modes (int)
// Return: Offset 0 = return status (int) / fd
static void* fileXioRpc_Open(unsigned int* sbuff)
{
	int ret;
	struct fxio_open_packet *packet=(struct fxio_open_packet*)sbuff;

	M_DEBUG("Open Request '%s' f:0x%x m:0x%x\n", packet->pathname, packet->flags, packet->mode);
	ret=iomanX_open(packet->pathname, packet->flags, packet->mode);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fd (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Close(unsigned int* sbuff)
{
	int ret;
	struct fxio_close_packet *packet=(struct fxio_close_packet*)sbuff;

	M_DEBUG("Close Request fd:%d\n", packet->fd);
	ret=iomanX_close(packet->fd);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fd (int)
// Send:   Offset 4 = pointer to buffer in EE mem
// Send:   Offset 8 = buffer size (int)
// Send:   Offset 12 = pointer to intr_data in EE mem
static void* fileXioRpc_Read(unsigned int* sbuff)
{
	int ret;
	struct fxio_read_packet *packet=(struct fxio_read_packet*)sbuff;

	M_DEBUG("Read Request fd:%d, size:%d\n", packet->fd, packet->size);
	ret=fileXio_Read_RPC(packet->fd, packet->buffer, packet->size, packet->intrData);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fd (int)
// Send:   Offset 4 = pointer to buffer in EE mem
// Send:   Offset 8 = buffer size (int)
// Send:   Offset 12 = misaligned buffer size (int)
// Send:   Offset 16 = misaligned buffer (16)
static void* fileXioRpc_Write(unsigned int* sbuff)
{
	int ret;
	struct fxio_write_packet *packet=(struct fxio_write_packet*)sbuff;

	M_DEBUG("Write Request fd:%d, size:%d\n", packet->fd, packet->size);
	ret=fileXio_Write_RPC(packet->fd, packet->buffer, packet->size,
                            packet->unalignedDataLen, packet->unalignedData);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fd (int)
// Send:   Offset 4 = offset (long)
// Send:   Offset 8 = whence (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Lseek(unsigned int* sbuff)
{
	int ret;
	struct fxio_lseek_packet *packet=(struct fxio_lseek_packet*)sbuff;

	M_DEBUG("Lseek Request fd:%d off:%d mode:%d\n", packet->fd, packet->offset, packet->whence);
	ret=iomanX_lseek(packet->fd, (long int)packet->offset, packet->whence);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fd (int)
// Send:   Offset 4 = offset (long long)
// Send:   Offset 12 = whence (int)
// Return: Offset 0 = return status (long long)
static void* fileXioRpc_Lseek64(unsigned int* sbuff)
{
	long long ret;
	struct fxio_lseek64_packet *packet=(struct fxio_lseek64_packet*)sbuff;
	struct fxio_lseek64_return_pkt *ret_packet=(struct fxio_lseek64_return_pkt*)sbuff;

	M_DEBUG("Lseek64 Request...\n");

	long long offsetHI = packet->offset_hi;
	offsetHI = offsetHI << 32;
	long long offset = offsetHI | packet->offset_lo;
	M_DEBUG("Lseek64 Request fd:%d off:%lld, mode:%d\n", packet->fd, offset, packet->whence);

	ret=iomanX_lseek64(packet->fd, offset, packet->whence);
	ret_packet->pos_lo = (u32)(ret & 0xffffffff);
	ret_packet->pos_hi = (u32)((ret >> 32) & 0xffffffff);

	return sbuff;
}

// Send:   Offset 0 = filename (int)
// Send:   Offset 512 = pointer to EE mem
// Send:   Offset 516 = mask (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_ChStat(unsigned int* sbuff)
{
	int ret=-1;
	struct fxio_chstat_packet *packet=(struct fxio_chstat_packet*)sbuff;

	M_DEBUG("ChStat Request '%s'\n", packet->pathname);
    ret=fileXio_chstat_RPC(packet->pathname, packet->stat, packet->mask);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = filename
// Send:   Offset 512 = pointer to EE mem
// Return: Offset 0 = return status (int)
static void* fileXioRpc_GetStat(unsigned int* sbuff)
{
	int ret=-1;
	struct fxio_getstat_packet *packet=(struct fxio_getstat_packet*)sbuff;

	M_DEBUG("GetStat Request '%s'\n", packet->pathname);
	ret=fileXio_getstat_RPC(packet->pathname, packet->stat);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = device
// Send:   Offset 128 = blockdevice
// Send:   Offset 640 = args
// Send:   Offset 1152 = arglen (int)
// Return: Offset 0 = return status
static void* fileXioRpc_Format(unsigned int* sbuff)
{
	int ret;
	struct fxio_format_packet *packet=(struct fxio_format_packet*)sbuff;

	M_DEBUG("Format Request dev:'%s' bd:'%s'\n", packet->device, packet->blockDevice);
	ret=iomanX_format(packet->device, packet->blockDevice, packet->args, packet->arglen);
	sbuff[0] = ret;
	return sbuff;
}

//int io_AddDrv(iomanX_iop_device_t *device);
static void* fileXioRpc_AddDrv(unsigned int* sbuff)
{
	int ret=-1;
	M_DEBUG("AddDrv Request\n");
	//	BODY
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = fsname
// Return: Offset 0 = return status (int)
static void* fileXioRpc_DelDrv(unsigned int* sbuff)
{
	int ret=-1;
	M_DEBUG("DelDrv Request\n");
//	ret=deldrv((char *)&sbuff[0/4]);
	sbuff[0] = ret;
	return sbuff;
}

// Send:   Offset 0 = devname
// Send:   Offset 512 = flag (int)
// Return: Offset 0 = return status (int)
static void* fileXioRpc_Sync(unsigned int* sbuff)
{
	int ret;
	struct fxio_sync_packet *packet=(struct fxio_sync_packet*)sbuff;

	M_DEBUG("Sync Request\n");
	ret=iomanX_sync(packet->device, packet->flags);
	sbuff[0] = ret;
	return sbuff;
}

//int io_devctl(const char *name, int cmd, void *arg, unsigned int arglen, void *bufp,
//		unsigned int buflen);
static void* fileXioRpc_Devctl(unsigned int* sbuff)
{
	struct fxio_devctl_packet *packet = (struct fxio_devctl_packet *)sbuff;
	struct fxio_ctl_return_pkt *ret_buf = (struct fxio_ctl_return_pkt *)rwbuf;
	SifDmaTransfer_t dmatrans;
	int intStatus;
	int ret;

	M_DEBUG("Devctl Request '%s' cmd:%d\n", packet->name, packet->cmd);

	ret = iomanX_devctl(packet->name, packet->cmd, packet->arg, packet->arglen, ret_buf->buf, packet->buflen);

	// Transfer buffer back to EE
	if(packet->buflen != 0)
	{
		dmatrans.src = ret_buf;
		dmatrans.dest = packet->intr_data;
		dmatrans.attr = 0;
		dmatrans.size = sizeof(struct fxio_ctl_return_pkt);

		ret_buf->dest = packet->buf;

		// EE is expecting data.. on error, simply use size of 0 so no data is copied.
		if(ret >= 0)
			ret_buf->len = packet->buflen;
		else
			ret_buf->len = 0;

		CpuSuspendIntr(&intStatus);
		sceSifSetDma(&dmatrans, 1);
		CpuResumeIntr(intStatus);
	}

	sbuff[0] = ret;
	return sbuff;
}

//int io_ioctl(int fd, int cmd, void *arg);
static void* fileXioRpc_Ioctl(unsigned int* sbuff)
{
	struct fxio_ioctl_packet *packet = (struct fxio_ioctl_packet *)sbuff;
	int ret;

	M_DEBUG("Ioctl Request fd:%d cmd:%d\n", packet->fd, packet->cmd);

	//	BODY
	ret=iomanX_ioctl(packet->fd, packet->cmd, packet->arg);
	sbuff[0] = ret;
	return sbuff;
}
//int io_ioctl2(int fd, int cmd, void *arg, unsigned int arglen, void *bufp,
//		unsigned int buflen);
static void* fileXioRpc_Ioctl2(unsigned int* sbuff)
{
	struct fxio_ioctl2_packet *packet = (struct fxio_ioctl2_packet *)sbuff;
	struct fxio_ctl_return_pkt *ret_buf = (struct fxio_ctl_return_pkt *)rwbuf;
	SifDmaTransfer_t dmatrans;
	int intStatus;
	int ret;

	M_DEBUG("ioctl2 Request fd:%d cmd:%d\n", packet->fd, packet->cmd);

	ret = iomanX_ioctl2(packet->fd, packet->cmd, packet->arg, packet->arglen, ret_buf->buf, packet->buflen);

	// Transfer buffer back to EE
	if(packet->buflen != 0)
	{
		dmatrans.src = ret_buf;
		dmatrans.dest = packet->intr_data;
		dmatrans.attr = 0;
		dmatrans.size = sizeof(struct fxio_ctl_return_pkt);

		ret_buf->dest = packet->buf;

		// EE is expecting data.. on error, simply use size of 0 so no data is copied.
		if(ret >= 0)
			ret_buf->len = packet->buflen;
		else
			ret_buf->len = 0;

		CpuSuspendIntr(&intStatus);
		sceSifSetDma(&dmatrans, 1);
		CpuResumeIntr(intStatus);
	}

	sbuff[0] = ret;
	return sbuff;
}

static void* fileXioRpc_Dopen(unsigned int* sbuff)
{
	int ret;
	struct fxio_pathsel_packet *packet=(struct fxio_pathsel_packet*)sbuff;

	M_DEBUG("Dopen Request '%s'\n", packet->pathname);
	ret=iomanX_dopen(packet->pathname);
	sbuff[0] = ret;
	return sbuff;
}

static void* fileXioRpc_Dread(unsigned int* sbuff)
{
	int ret=-1;
	struct fxio_dread_packet *packet=(struct fxio_dread_packet*)sbuff;

	M_DEBUG("Dread Request fd:%d\n", packet->fd);
	ret=fileXio_dread_RPC(packet->fd, packet->dirent);
	sbuff[0] = ret;
	return sbuff;
}

static void* fileXioRpc_Dclose(unsigned int* sbuff)
{
	int ret;
	struct fxio_close_packet *packet=(struct fxio_close_packet*)sbuff;

	M_DEBUG("Dclose Request fd:%d\n", packet->fd);
	ret=iomanX_dclose(packet->fd);
	sbuff[0] = ret;
	return sbuff;
}

/*************************************************
* The functions below are for internal use only, *
* and are not to be exported                     *
*************************************************/

static void fileXio_Thread(void* param)
{
	int OldState;

	(void)param;

	M_PRINTF("fileXio: fileXio RPC Server v1.00\nCopyright (c) 2003 adresd\n");
	M_DEBUG("fileXio: RPC Initialize\n");

	sceSifInitRpc(0);

	RWBufferSize=DEFAULT_RWSIZE;
	CpuSuspendIntr(&OldState);
	rwbuf = AllocSysMemory(ALLOC_FIRST, RWBufferSize, NULL);
	CpuResumeIntr(OldState);
	if (rwbuf == NULL)
	{
		M_DEBUG("Failed to allocate memory for RW buffer!\n");

		SleepThread();
	}

	sceSifSetRpcQueue(&qd, GetThreadId());
	sceSifRegisterRpc(&sd0, FILEXIO_IRX, &fileXio_rpc_server, fileXio_rpc_buffer, NULL, NULL, &qd);
	sceSifRpcLoop(&qd);
}

static void* filexioRpc_SetRWBufferSize(void *sbuff)
{
	int OldState;

	if(rwbuf!=NULL){
		CpuSuspendIntr(&OldState);
		FreeSysMemory(rwbuf);
		CpuResumeIntr(OldState);
	}

	RWBufferSize=((struct fxio_rwbuff*)sbuff)->size;
	CpuSuspendIntr(&OldState);
	rwbuf=AllocSysMemory(ALLOC_FIRST, RWBufferSize, NULL);
	CpuResumeIntr(OldState);

	((int*)sbuff)[0] = rwbuf!=NULL?0:-ENOMEM;
	return sbuff;
}

static void* fileXio_rpc_server(int fno, void *data, int size)
{
	(void)size;

	switch(fno) {
		case FILEXIO_DOPEN:
			return fileXioRpc_Dopen((unsigned*)data);
		case FILEXIO_DREAD:
			return fileXioRpc_Dread((unsigned*)data);
		case FILEXIO_DCLOSE:
			return fileXioRpc_Dclose((unsigned*)data);
		case FILEXIO_MOUNT:
			return fileXioRpc_Mount((unsigned*)data);
		case FILEXIO_UMOUNT:
			return fileXioRpc_uMount((unsigned*)data);
		case FILEXIO_GETDIR:
			return fileXioRpc_Getdir((unsigned*)data);
		case FILEXIO_COPYFILE:
			return fileXioRpc_CopyFile((unsigned*)data);
		case FILEXIO_STOP:
			return fileXioRpc_Stop();
		case FILEXIO_CHDIR:
			return fileXioRpc_ChDir((unsigned*)data);
		case FILEXIO_RENAME:
			return fileXioRpc_Rename((unsigned*)data);
		case FILEXIO_MKDIR:
			return fileXioRpc_MkDir((unsigned*)data);
		case FILEXIO_RMDIR:
			return fileXioRpc_RmDir((unsigned*)data);
		case FILEXIO_REMOVE:
			return fileXioRpc_Remove((unsigned*)data);
		case FILEXIO_OPEN:
			return fileXioRpc_Open((unsigned*)data);
		case FILEXIO_CLOSE:
			return fileXioRpc_Close((unsigned*)data);
		case FILEXIO_READ:
			return fileXioRpc_Read((unsigned*)data);
		case FILEXIO_WRITE:
			return fileXioRpc_Write((unsigned*)data);
		case FILEXIO_LSEEK:
			return fileXioRpc_Lseek((unsigned*)data);
		case FILEXIO_IOCTL:
			return fileXioRpc_Ioctl((unsigned*)data);
		case FILEXIO_GETSTAT:
			return fileXioRpc_GetStat((unsigned*)data);
		case FILEXIO_CHSTAT:
			return fileXioRpc_ChStat((unsigned*)data);
		case FILEXIO_FORMAT:
			return fileXioRpc_Format((unsigned*)data);
		case FILEXIO_ADDDRV:
			return fileXioRpc_AddDrv((unsigned*)data);
		case FILEXIO_DELDRV:
			return fileXioRpc_DelDrv((unsigned*)data);
		case FILEXIO_SYNC:
			return fileXioRpc_Sync((unsigned*)data);
		case FILEXIO_DEVCTL:
			return fileXioRpc_Devctl((unsigned*)data);
		case FILEXIO_SYMLINK:
			return fileXioRpc_SymLink((unsigned*)data);
		case FILEXIO_READLINK:
			return fileXioRpc_ReadLink((unsigned*)data);
		case FILEXIO_IOCTL2:
			return fileXioRpc_Ioctl2((unsigned*)data);
		case FILEXIO_LSEEK64:
			return fileXioRpc_Lseek64((unsigned*)data);
		case FILEXIO_GETDEVICELIST:
			return fileXioRpc_GetDeviceList((unsigned*)data);
		case FILEXIO_SETRWBUFFSIZE:
			return filexioRpc_SetRWBufferSize(data);
	}
	return NULL;
}


// Copy a DIR Entry from the native format to our format
static void DirEntryCopy(struct fileXioDirEntry* dirEntry, iox_dirent_t* internalDirEntry)
{
	dirEntry->fileSize = internalDirEntry->stat.size;
	dirEntry->fileProperties = internalDirEntry->stat.attr;
	strncpy(dirEntry->filename,internalDirEntry->name,127);
	dirEntry->filename[127] = '\0';
}
