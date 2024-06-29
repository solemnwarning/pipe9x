/* Pipe9X - Anonymous pipes with overlapped I/O semantics on Windows 9x
 * Copyright (C) 2024 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of the copyright holder nor the names of its
 *        contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "pipe9x.h"

struct PipeData
{
	HANDLE pipe;
	unsigned char *rw_buf;
	size_t rw_buf_size;
	OVERLAPPED overlapped;
	BOOL pending;
	
	BOOL use_thread_fallback;
	HANDLE io_thread;
	DWORD bytes_transferred;
	DWORD io_result;
};

struct _PipeReadHandle
{
	struct PipeData data;
};

struct _PipeWriteHandle
{
	struct PipeData data;
};

DWORD pipe9x_create(
	PipeReadHandle *prh_out,
	size_t read_size,
	LPSECURITY_ATTRIBUTES read_security,
	PipeWriteHandle *pwh_out,
	size_t write_size,
	LPSECURITY_ATTRIBUTES write_security)
{
	*prh_out = NULL;
	*pwh_out = NULL;
	
	PipeReadHandle prh  = malloc(sizeof(struct _PipeReadHandle));
	PipeWriteHandle pwh = malloc(sizeof(struct _PipeWriteHandle));
	
	if(prh == NULL || pwh == NULL)
	{
		free(pwh);
		free(prh);
		
		return ERROR_OUTOFMEMORY;
	}
	
	prh->data.pipe = INVALID_HANDLE_VALUE;
	prh->data.rw_buf = malloc(read_size);
	prh->data.rw_buf_size = read_size;
	prh->data.overlapped.hEvent = NULL;
	prh->data.pending = FALSE;
	prh->data.use_thread_fallback = FALSE;
	prh->data.io_thread = NULL;
	
	pwh->data.pipe = INVALID_HANDLE_VALUE;
	pwh->data.rw_buf = malloc(read_size);
	pwh->data.rw_buf_size = read_size;
	pwh->data.overlapped.hEvent = NULL;
	pwh->data.pending = FALSE;
	pwh->data.use_thread_fallback = FALSE;
	pwh->data.io_thread = NULL;
	
	if(prh->data.rw_buf == NULL || pwh->data.rw_buf == NULL)
	{
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return ERROR_OUTOFMEMORY;
	}
	
	/* Create event objects used to signal overlapped I/O completion. */
	
	prh->data.overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(prh->data.overlapped.hEvent == NULL)
	{
		DWORD error = GetLastError();
		
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return error;
	}
	
	pwh->data.overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(pwh->data.overlapped.hEvent == NULL)
	{
		DWORD error = GetLastError();
		
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return error;
	}
	
	/* Create named pipe to serve as the read end of the pipe.
	 *
	 * Anonymous pipes cannot be used for overlapped I/O, so we need to
	 * create a named pipe with a random name and open it.
	*/
	
	char pipename[32];
	
	while(prh->data.pipe == INVALID_HANDLE_VALUE)
	{
		strcpy(pipename, "\\\\.\\pipe\\tmp_");
		int pnlen = strlen(pipename);
		
		while(pnlen < (sizeof(pipename) - 1))
		{
			char rndchar = 'A' + (rand() % 26);
			pipename[pnlen++] = rndchar;
		}
		
		pipename[pnlen] = '\0';
		
		prh->data.pipe = CreateNamedPipe(
			pipename,                                           /* lpName */
			(PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED),       /* dwOpenMode */
			(PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT),  /* dwPipeMode */
			1,                                                  /* nMaxInstances */
			read_size,                                          /* nOutBufferSize */
			read_size,                                          /* nInBufferSize */
			0,                                                  /* nDefaultTimeOut */
			read_security);                                     /* lpSecurityAttributes */
		
		if(prh->data.pipe == INVALID_HANDLE_VALUE)
		{
			DWORD error = GetLastError();
			
			if(error == ERROR_FILE_EXISTS)
			{
				/* Name already in use, loop and try another. */
			}
			else if(error == ERROR_CALL_NOT_IMPLEMENTED)
			{
				/* Okay... named pipes only exist on Windows NT, so we have to
				 * fall back to using anonymous pipes, which means we can't use
				 * overlapped I/O and have to fall back to performing read/write
				 * operations on a background thread instead.
				 *
				 * Wheeee.
				*/
				
				if(!CreatePipe(&(prh->data.pipe), &(pwh->data.pipe), NULL, read_size))
				{
					error = GetLastError();
					
					pipe9x_write_close(pwh);
					pipe9x_read_close(prh);
					
					return error;
				}
				
				prh->data.use_thread_fallback = TRUE;
				pwh->data.use_thread_fallback = TRUE;
				
				*prh_out = prh;
				*pwh_out = pwh;
				
				return ERROR_SUCCESS;
			}
			else{
				pipe9x_write_close(pwh);
				pipe9x_read_close(prh);
				
				return error;
			}
		}
	}
	
	if(!ConnectNamedPipe(prh->data.pipe, &(prh->data.overlapped)))
	{
		DWORD error = GetLastError();
		
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return error;
	}
	
	/* Make a connection to the pipe to serve as the write end. */
	
	pwh->data.pipe = CreateFile(
		pipename,              /* lpFileName */
		GENERIC_WRITE,         /* dwDesiredAccess */
		0,                     /* dwShareMode */
		write_security,        /* lpSecurityAttributes */
		OPEN_EXISTING,         /* dwCreationDisposition */
		FILE_FLAG_OVERLAPPED,  /* dwFlagsAndAttributes */
		NULL);                 /* hTemplateFile */
	
	if(pwh->data.pipe == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return error;
	}
	
	/* Complete the connection on the read end. */
	
	DWORD transferred_bytes;
	if(!GetOverlappedResult(prh->data.pipe, &(prh->data.overlapped), &transferred_bytes, TRUE))
	{
		DWORD error = GetLastError();
		
		pipe9x_write_close(pwh);
		pipe9x_read_close(prh);
		
		return error;
	}
	
	*prh_out = prh;
	*pwh_out = pwh;
	
	return ERROR_SUCCESS;
}

static void _pipe9x_cleanup(struct PipeData *pd)
{
	if(pd->pipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pd->pipe);
		pd->pipe = NULL;
	}
	
	if(pd->pending)
	{
		if(pd->use_thread_fallback)
		{
			assert(pd->io_thread != NULL);
			
			WaitForSingleObject(pd->io_thread, INFINITE);
			CloseHandle(pd->io_thread);
			
			pd->io_thread = NULL;
		}
		else{
			assert(pd->overlapped.hEvent != NULL);
			WaitForSingleObject(pd->overlapped.hEvent, INFINITE);
		}
	}
	
	if(pd->overlapped.hEvent != NULL)
	{
		CloseHandle(pd->overlapped.hEvent);
		pd->overlapped.hEvent = NULL;
	}
}

void pipe9x_read_close(PipeReadHandle prh)
{
	if(prh == NULL)
	{
		return;
	}
	
	_pipe9x_cleanup(&(prh->data));
	free(prh);
}

static DWORD WINAPI _pipe9x_read_thread(LPVOID lpParameter)
{
	PipeReadHandle prh = (PipeReadHandle)(lpParameter);
	
	if(ReadFile(
		prh->data.pipe,
		prh->data.rw_buf,
		prh->data.rw_buf_size,
		&(prh->data.bytes_transferred),
		NULL))
	{
		prh->data.io_result = ERROR_SUCCESS;
	}
	else{
		prh->data.io_result = GetLastError();
	}
	
	SetEvent(prh->data.overlapped.hEvent);
	
	return 0;
}

DWORD pipe9x_read_initiate(PipeReadHandle prh)
{
	assert(prh != NULL);
	
	if(prh->data.pending)
	{
		return ERROR_IO_PENDING;
	}
	
	if(prh->data.use_thread_fallback)
	{
		assert(prh->data.io_thread == NULL);
		
		ResetEvent(prh->data.overlapped.hEvent);
		
		DWORD io_thread_id;
		prh->data.io_thread = CreateThread(NULL, 0, &_pipe9x_read_thread, prh, 0, &io_thread_id);
		
		if(prh->data.io_thread == NULL)
		{
			return GetLastError();
		}
		
		prh->data.pending = TRUE;
		
		return ERROR_IO_PENDING;
	}
	else{
		if(ReadFile(
			prh->data.pipe,
			prh->data.rw_buf,
			prh->data.rw_buf_size,
			&(prh->data.bytes_transferred),
			&(prh->data.overlapped)))
		{
			/* Not sure if this is actually a valid result for overlapped
			 * operations, but lets assume it is...
			*/
			
			prh->data.pending = TRUE;
			return ERROR_IO_PENDING;
		}
		else{
			DWORD error = GetLastError();
			
			if(error == ERROR_IO_PENDING)
			{
				prh->data.pending = TRUE;
				return ERROR_IO_PENDING;
			}
			else{
				return error;
			}
		}
	}
}

DWORD pipe9x_read_result(PipeReadHandle prh, void **data_out, size_t *data_size_out, BOOL wait)
{
	assert(prh != NULL);
	
	if(!prh->data.pending)
	{
		return ERROR_INVALID_PARAMETER;
	}
	
	if(prh->data.use_thread_fallback)
	{
		assert(prh->data.io_thread != NULL);
		
		DWORD wait_result = WaitForSingleObject(prh->data.overlapped.hEvent, (wait ? INFINITE : 0));
		if(wait_result == WAIT_OBJECT_0)
		{
			wait_result = WaitForSingleObject(prh->data.io_thread, INFINITE);
			assert(wait_result == WAIT_OBJECT_0);
			
			prh->data.pending = FALSE;
			
			CloseHandle(prh->data.io_thread);
			prh->data.io_thread = NULL;
			
			if(prh->data.io_result == ERROR_SUCCESS)
			{
				*data_out = prh->data.rw_buf;
				*data_size_out = prh->data.bytes_transferred;
			}
			
			return prh->data.io_result;
		}
		else if(wait_result == WAIT_TIMEOUT)
		{
			return ERROR_IO_INCOMPLETE;
		}
		else{
			/* WaitForSingleObject() shouldn't fail here... */
			abort();
		}
	}
	
	DWORD bytes_transferred;
	if(GetOverlappedResult(prh->data.pipe, &(prh->data.overlapped), &bytes_transferred, wait))
	{
		prh->data.pending = FALSE;
		
		*data_out = prh->data.rw_buf;
		*data_size_out = bytes_transferred;
		
		return ERROR_SUCCESS;
	}
	else{
		DWORD error = GetLastError();
		
		/* Clear the pending flag if the operation has failed. */
		if(error != ERROR_IO_INCOMPLETE)
		{
			prh->data.pending = FALSE;
		}
		
		return error;
	}
}

BOOL pipe9x_read_pending(PipeReadHandle prh)
{
	assert(prh != NULL);
	return prh->data.pending;
}

HANDLE pipe9x_read_pipe(PipeReadHandle prh)
{
	assert(prh != NULL);
	return prh->data.pipe;
}

HANDLE pipe9x_read_event(PipeReadHandle prh)
{
	assert(prh != NULL);
	return prh->data.overlapped.hEvent;
}

void pipe9x_write_close(PipeWriteHandle pwh)
{
	if(pwh == NULL)
	{
		return;
	}
	
	_pipe9x_cleanup(&(pwh->data));
	free(pwh);
}

BOOL pipe9x_write_pending(PipeWriteHandle pwh)
{
	assert(pwh != NULL);
	return pwh->data.pending;
}

HANDLE pipe9x_write_pipe(PipeWriteHandle pwh)
{
	assert(pwh != NULL);
	return pwh->data.pipe;
}

HANDLE pipe9x_write_event(PipeWriteHandle pwh)
{
	assert(pwh != NULL);
	return pwh->data.overlapped.hEvent;
}
