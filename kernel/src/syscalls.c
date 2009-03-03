/**
 * @version		$Id$
 * @author		Nils Asmussen <nils@script-solution.de>
 * @copyright	2008 Nils Asmussen
 */

#include "../h/common.h"
#include "../h/syscalls.h"
#include "../h/intrpt.h"
#include "../h/proc.h"
#include "../h/vfs.h"
#include "../h/paging.h"
#include "../h/util.h"
#include "../h/debug.h"
#include "../h/kheap.h"
#include <video.h>
#include <string.h>

#define SYSCALL_COUNT 20

/* some convenience-macros */
#define SYSC_ERROR(stack,errorCode) ((stack)->number = (errorCode))
#define SYSC_RET1(stack,val) ((stack)->arg1 = (val))
#define SYSC_RET2(stack,val) ((stack)->arg2 = (val))

/* syscall-handlers */
typedef void (*fSyscall)(sSysCallStack *stack);

/* for syscall-definitions */
typedef struct {
	fSyscall handler;
	u8 argCount;
} sSyscall;

/**
 * Returns the pid of the current process
 *
 * @return tPid the pid
 */
static void sysc_getpid(sSysCallStack *stack);
/**
 * Returns the parent-pid of the current process
 *
 * @return tPid the parent-pid
 */
static void sysc_getppid(sSysCallStack *stack);
/**
 * Temporary syscall to print out a character
 */
static void sysc_debugc(sSysCallStack *stack);
/**
 * Clones the current process
 *
 * @return tPid 0 for the child, the child-pid for the parent-process
 */
static void sysc_fork(sSysCallStack *stack);
/**
 * Destroys the process and issues a context-switch
 *
 * @param u32 the exit-code
 */
static void sysc_exit(sSysCallStack *stack);
/**
 * Open-syscall. Opens a given path with given mode and returns the file-descriptor to use
 * to the user.
 *
 * @param cstring the vfs-filename
 * @param u32 flags (read / write)
 * @return tFD the file-descriptor
 */
static void sysc_open(sSysCallStack *stack);
/**
 * Read-syscall. Reads a given number of bytes in a given file at the current position
 *
 * @param tFD file-descriptor
 * @param void* buffer
 * @param u32 number of bytes
 * @return u32 the number of read bytes
 */
static void sysc_read(sSysCallStack *stack);
/**
 * Write-syscall. Writes a given number of bytes to a given file at the current position
 *
 * @param tFD file-descriptor
 * @param void* buffer
 * @param u32 number of bytes
 * @return u32 the number of written bytes
 */
static void sysc_write(sSysCallStack *stack);
/**
 * Sends an "End-Of-Transfer" for the given file-descriptor. This will release the lock for
 * service-usages so that the other side can start working.
 *
 * @param tFD the file-descriptor
 * @return s32 0 on success
 */
static void sysc_sendEOT(sSysCallStack *stack);
/**
 * Duplicates the given file-descriptor
 *
 * @param tFD the file-descriptor
 * @return s32 the error-code or the new file-descriptor
 */
static void sysc_dupFd(sSysCallStack *stack);
/**
 * Redirects <src> to <dst>. <src> will be closed. Note that both fds have to exist!
 *
 * @param tFD src the source-file-descriptor
 * @param tFD dst the destination-file-descriptor
 * @return s32 the error-code or 0 if successfull
 */
static void sysc_redirFd(sSysCallStack *stack);
/**
 * Closes the given file-descriptor
 *
 * @param tFD the file-descriptor
 */
static void sysc_close(sSysCallStack *stack);
/**
 * Registers a service
 *
 * @param cstring name of the service
 * @return s32 0 if successfull
 */
static void sysc_regService(sSysCallStack *stack);
/**
 * Unregisters a service
 *
 * @param s32 the node-id
 * @return s32 0 on success or a negative error-code
 */
static void sysc_unregService(sSysCallStack *stack);
/**
 * For services: Waits until a clients wants to be served and returns a file-descriptor
 * for it.
 *
 * @param sVFSNode* node the service-node
 * @return tFD the file-descriptor to use
 */
static void sysc_waitForClient(sSysCallStack *stack);
/**
 * Changes the process-size
 *
 * @param u32 number of pages
 * @return u32 the previous number of text+data pages
 */
static void sysc_changeSize(sSysCallStack *stack);
/**
 * Maps physical memory in the virtual user-space
 *
 * @param u32 physical address
 * @param u32 number of bytes
 * @return void* the start-address
 */
static void sysc_mapPhysical(sSysCallStack *stack);
/**
 * Releases the CPU (reschedule)
 */
static void sysc_yield(sSysCallStack *stack);
/**
 * Requests some IO-ports
 *
 * @param u16 start-port
 * @param u16 number of ports
 * @return s32 0 if successfull or a negative error-code
 */
static void sys_requestIOPorts(sSysCallStack *stack);
/**
 * Releases some IO-ports
 *
 * @param u16 start-port
 * @param u16 number of ports
 * @return s32 0 if successfull or a negative error-code
 */
static void sys_releaseIOPorts(sSysCallStack *stack);

/* our syscalls */
static sSyscall syscalls[SYSCALL_COUNT] = {
	/* 0 */		{sysc_getpid,			0},
	/* 1 */		{sysc_getppid,			0},
	/* 2 */ 	{sysc_debugc,			1},
	/* 3 */		{sysc_fork,				0},
	/* 4 */ 	{sysc_exit,				1},
	/* 5 */ 	{sysc_open,				2},
	/* 6 */ 	{sysc_close,			1},
	/* 7 */ 	{sysc_read,				3},
	/* 8 */		{sysc_regService,		1},
	/* 9 */ 	{sysc_unregService,		0},
	/* 10 */	{sysc_changeSize,		1},
	/* 11 */	{sysc_mapPhysical,		2},
	/* 12 */	{sysc_write,			3},
	/* 13 */	{sysc_sendEOT,			1},
	/* 14 */	{sysc_yield,			0},
	/* 15 */	{sysc_waitForClient,	1},
	/* 16 */	{sys_requestIOPorts,	2},
	/* 17 */	{sys_releaseIOPorts,	2},
	/* 18 */	{sysc_dupFd,			1},
	/* 19 */	{sysc_redirFd,			2},
};

void sysc_handle(sSysCallStack *stack) {
	ASSERT(stack != NULL,"stack == NULL");

	u32 sysCallNo = (u32)stack->number;
	if(sysCallNo < SYSCALL_COUNT) {
		/* no error by default */
		SYSC_ERROR(stack,0);
		syscalls[sysCallNo].handler(stack);
	}
}

static void sysc_getpid(sSysCallStack *stack) {
	sProc *p = proc_getRunning();
	SYSC_RET1(stack,p->pid);
}

static void sysc_getppid(sSysCallStack *stack) {
	sProc *p = proc_getRunning();
	SYSC_RET1(stack,p->parentPid);
}

static void sysc_debugc(sSysCallStack *stack) {
	vid_putchar((s8)stack->arg1);
}

static void sysc_fork(sSysCallStack *stack) {
	tPid newPid = proc_getFreePid();
	s32 res = proc_clone(newPid);
	/* error? */
	if(res == -1) {
		SYSC_RET1(stack,-1);
	}
	/* child? */
	else if(res == 1) {
		SYSC_RET1(stack,0);
	}
	/* parent */
	else {
		SYSC_RET1(stack,newPid);
	}
}

static void sysc_exit(sSysCallStack *stack) {
	vid_printf("Process %d exited with exit-code %d\n",proc_getRunning()->pid,stack->arg1);
	proc_suicide();
	proc_switch();
}

static void sysc_open(sSysCallStack *stack) {
	s8 path[255];
	u8 flags;
	tVFSNodeNo nodeNo;
	tFD fd;
	s32 err;

	/* copy path */
	copyUserToKernel((u8*)stack->arg1,(u8*)path,MIN(strlen((string)stack->arg1) + 1,255));

	/* check flags */
	flags = ((u8)stack->arg2) & (VFS_WRITE | VFS_READ);
	if((flags & (VFS_READ | VFS_WRITE)) == 0) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* resolve path */
	err = vfs_resolvePath(path,&nodeNo);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	/* open file */
	err = vfs_openFile(flags,nodeNo,&fd);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	/*vfs_dbg_printTree();*/

	/* return fd */
	SYSC_RET1(stack,fd);
}

static void sysc_read(sSysCallStack *stack) {
	tFD fd = (tFD)stack->arg1;
	void *buffer = (void*)stack->arg2;
	u32 count = stack->arg3;
	s32 readBytes;

	/* validate count and buffer */
	if(count <= 0) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}
	if(!paging_isRangeUserWritable((u32)buffer,count)) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* read */
	readBytes = vfs_readFile(fd,buffer,count);
	if(readBytes < 0) {
		SYSC_ERROR(stack,readBytes);
		return;
	}

	SYSC_RET1(stack,readBytes);
}

static void sysc_write(sSysCallStack *stack) {
	tFD fd = (tFD)stack->arg1;
	void *buffer = (void*)stack->arg2;
	u32 count = stack->arg3;
	s32 writtenBytes;
	/* validate count and buffer */
	if(count <= 0) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}
	if(!paging_isRangeUserReadable((u32)buffer,count)) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* read */
	writtenBytes = vfs_writeFile(fd,buffer,count);
	if(writtenBytes < 0) {
		SYSC_ERROR(stack,writtenBytes);
		return;
	}

	SYSC_RET1(stack,writtenBytes);
}

static void sysc_sendEOT(sSysCallStack *stack) {
	tFD fd = (tFD)stack->arg1;
	s32 err = vfs_sendEOT(fd);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,0);
}

static void sysc_dupFd(sSysCallStack *stack) {
	tFD fd = (tFD)stack->arg1;
	s32 err = proc_dupFd(fd);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,err);
}

static void sysc_redirFd(sSysCallStack *stack) {
	tFD src = (tFD)stack->arg1;
	tFD dst = (tFD)stack->arg2;
	s32 err = proc_redirFd(src,dst);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,err);
}

static void sysc_close(sSysCallStack *stack) {
	tFD fd = (tFD)stack->arg1;
	vfs_closeFile(fd);
}

static void sysc_regService(sSysCallStack *stack) {
	sProc *p = proc_getRunning();
	s32 res;

	res = vfs_createService(p,(cstring)stack->arg1);
	if(res < 0) {
		SYSC_ERROR(stack,res);
		return;
	}

	/*vfs_dbg_printTree();*/

	SYSC_RET1(stack,res);
}

static void sysc_unregService(sSysCallStack *stack) {
	tVFSNodeNo no = stack->arg1;
	sProc *p = proc_getRunning();
	s32 err;

	/* check node-number */
	if(!vfs_isValidNodeNo(no)) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* remove the service */
	err = vfs_removeService(no);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,0);
}

static void sysc_waitForClient(sSysCallStack *stack) {
	tVFSNodeNo no = stack->arg1;
	s32 res;

	/* check argument */
	if(!vfs_isValidNodeNo(no)) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	res = vfs_waitForClient(no);
	if(res < 0) {
		SYSC_ERROR(stack,res);
		return;
	}

	SYSC_RET1(stack,res);
}

static void sysc_changeSize(sSysCallStack *stack) {
	s32 count = stack->arg1;
	sProc *p = proc_getRunning();
	u32 oldEnd = p->textPages + p->dataPages;

	/* check argument */
	if(count > 0) {
		if(!proc_segSizesValid(p->textPages,p->dataPages + count,p->stackPages)) {
			SYSC_ERROR(stack,ERR_NOT_ENOUGH_MEM);
			return;
		}
	}
	else if(count == 0) {
		SYSC_RET1(stack,oldEnd);
		return;
	}
	else if((unsigned)-count > p->dataPages) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* change size */
	if(proc_changeSize(count,CHG_DATA) == false) {
		SYSC_ERROR(stack,ERR_NOT_ENOUGH_MEM);
		return;
	}

	SYSC_RET1(stack,oldEnd);
}

static void sysc_mapPhysical(sSysCallStack *stack) {
	u32 addr,origPages;
	u32 phys = stack->arg1;
	u32 pages = BYTES_2_PAGES(stack->arg2);
	sProc *p = proc_getRunning();
	u32 i,*frames;

	/* trying to map memory in kernel area? */
	/* TODO is this ok? */
	if(phys + pages * PAGE_SIZE > KERNEL_P_ADDR) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	/* determine start-address */
	addr = (p->textPages + p->dataPages) * PAGE_SIZE;
	origPages = p->textPages + p->dataPages;

	/* not enough mem? */
	if(mm_getFreeFrmCount(MM_DEF) < paging_countFramesForMap(addr,pages)) {
		SYSC_ERROR(stack,ERR_NOT_ENOUGH_MEM);
		return;
	}

	/* invalid segment sizes? */
	if(!proc_segSizesValid(p->textPages,p->dataPages + pages,p->stackPages)) {
		SYSC_ERROR(stack,ERR_NOT_ENOUGH_MEM);
		return;
	}

	/* we have to allocate temporary space for the frame-address :( */
	frames = kheap_alloc(sizeof(u32) * pages);
	if(frames == NULL) {
		SYSC_ERROR(stack,ERR_NOT_ENOUGH_MEM);
		return;
	}

	for(i = 0; i < pages; i++) {
		frames[i] = phys / PAGE_SIZE;
		phys += PAGE_SIZE;
	}
	/* map the physical memory and free the temporary memory */
	paging_map(addr,frames,pages,PG_WRITABLE,false);
	kheap_free(frames);

	/* increase datapages */
	p->dataPages += pages;
	/* return start-addr */
	SYSC_RET1(stack,addr);
}

static void sysc_yield(sSysCallStack *stack) {
	proc_switch();
}

static void sys_requestIOPorts(sSysCallStack *stack) {
	u16 start = stack->arg1;
	u16 count = stack->arg2;

	/* check range */
	if(count == 0 || (u32)start + (u32)count > 0xFFFF) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	s32 err = proc_requestIOPorts(start,count);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,0);
}

static void sys_releaseIOPorts(sSysCallStack *stack) {
	u16 start = stack->arg1;
	u16 count = stack->arg2;

	/* check range */
	if(count == 0 || (u32)start + (u32)count > 0xFFFF) {
		SYSC_ERROR(stack,ERR_INVALID_SYSC_ARGS);
		return;
	}

	s32 err = proc_releaseIOPorts(start,count);
	if(err < 0) {
		SYSC_ERROR(stack,err);
		return;
	}

	SYSC_RET1(stack,0);
}
