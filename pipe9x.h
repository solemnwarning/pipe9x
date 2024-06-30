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

#ifndef PIPE9X_H
#define PIPE9X_H

#include <windows.h>

typedef struct _PipeWriteHandle *PipeWriteHandle;
typedef struct _PipeReadHandle *PipeReadHandle;

/**
 * @brief Create a pair of connected pipe handles.
 *
 * @param prh_out        Pointer to PipeReadHandle to receive read handle.
 * @param read_size      Size of pipe read buffer.
 * @param read_inherit   Whether the pipe read handle is inherited by new processes (ignored on 9x).
 * @param pwh_out        Pointer to PipeWriteHandle to receive write handle.
 * @param write_size     Size of pipe write buffer.
 * @param write_inherit  Whether the pipe write handle is inherited by new processes (ignored on 9x).
 *
 * @return ERROR_SUCCESS, or another win32 error code.
 *
 * This function creates a pair of connected pipe handles and other supporting
 * handles/structures wrapped in a PipeReadHandle and PipeWriteHandle object.
 *
 * On success, ERROR_SUCCESS is returned and *prh_out and *pwh_out are both
 * initialised to non-NULL values.
 *
 * On error, a win32 error code is returned and *prh_out and *pwh_out are both
 * initialised to NULL.
 *
 * The buffer size parameters specify the size of the internal read/write
 * buffers to allocate and set the upper size limit for read/write operations.
*/
DWORD pipe9x_create(
	PipeReadHandle *prh_out,
	size_t read_size,
	BOOL read_inherit,
	PipeWriteHandle *pwh_out,
	size_t write_size,
	BOOL write_inherit);

/**
 * @brief Closes the read end of a pipe created by pipe9x_create().
 *
 * @param prh  PipeReadHandle object to destroy (may be NULL).
*/
void pipe9x_read_close(PipeReadHandle prh);

/**
 * @brief Start a read in the background.
 *
 * This function will initiate an asynchronous read from the pipe into the
 * internal buffer of the PipeReadHandle object.
 *
 * On success, this function returns ERROR_IO_PENDING (for consistensy with
 * Win32 API functions) and completion can be polled using the
 * pipe9x_read_result() function or waited on using the event object returned
 * by pipe9x_read_event().
 *
 * Only one read operation can be pending at a time, attempting to start a
 * second read before the first one is completed using pipe9x_read_result()
 * will return ERROR_IO_INCOMPLETE.
 *
 * On Windows NT, this function uses overlapped I/O, on Windows 9x, a blocking
 * read is performed in a background thread instead.
*/
DWORD pipe9x_read_initiate(PipeReadHandle prh);

/**
 * @brief Get the result from a read operation.
 *
 * @param prh            PipeReadHandle object to check status of.
 * @param data_out       Pointer to receive pointer to read data.
 * @param data_size_out  Pointer to receive read data size.
 * @param wait           Whether to wait for completion before returning.
 *
 * This function gets the result of a read operation previous started using
 * the pipe9x_read_initiate() function.
 *
 * If the read completed successfully, ERROR_SUCCESS is returned, *data_out is
 * initialised with a pointer to the read data and *data_size_out contains the
 * number of bytes. The data remains valid until the PipeReadHandle object is
 * destroyed or pipe9x_read_initiate() is called again.
 *
 * If no data has been read yet, and wait is FALSE, ERROR_IO_INCOMPLETE will be
 * returned.
 *
 * If any other error occurs, the relevant Win32 error code is returned.
*/
DWORD pipe9x_read_result(PipeReadHandle prh, void **data_out, size_t *data_size_out, BOOL wait);

/**
 * @brief Check if a read operation is pending.
 *
 * This function checks if a read operation is pending. It will return true
 * from the point pipe9x_read_initiate() is called until a call to
 * pipe9x_read_result() which does not return ERROR_IO_PENDING is made.
*/
BOOL pipe9x_read_pending(PipeReadHandle prh);

/**
 * @brief Get the underlying Windows HANDLE of the pipe.
 *
 * This function obtains the underlying Windows HANDLE of the pipe, this is
 * intended for passing one end of the pipe into another process or library.
 *
 * Once the handle has been passed into a child process or duplicated, the
 * original PipeReadHandle object may be destroyed with pipe9x_read_close().
*/
HANDLE pipe9x_read_pipe(PipeReadHandle prh);

/**
 * @brief Get an event object for detecting I/O completion.
 *
 * This function obtains a HANDLE to an event object which can be used to
 * detect when a read operation has finished.
 *
 * The returned handle may be waited on, but must not be manually reset, set or
 * otherwise altered.
*/
HANDLE pipe9x_read_event(PipeReadHandle prh);

/**
 * @brief Closes the write end of a pipe created by pipe9x_create().
 *
 * @param pwh  PipeWriteHandle object to destroy (may be NULL).
*/
void pipe9x_write_close(PipeWriteHandle pwh);

/**
 * @brief Start a write in the background.
 *
 * @param pwh        PipeWriteHandle to write to.
 * @param data       Pointer to data buffer.
 * @param data_size  Size of data buffer.
 *
 * This function will initiate an asynchronous write to the pipe from the
 * internal buffer of the PipeWriteHandle object. The provided data is copied
 * into the internal buffer before the function returns.
 *
 * On success, this function returns ERROR_IO_PENDING (for consistensy with
 * Win32 API functions) and completion can be polled using the
 * pipe9x_write_result() function or waited on using the event object returned
 * by pipe9x_write_event().
 *
 * Only one write operation can be pending at a time, attempting to start a
 * second write before the first one is completed using pipe9x_write_result()
 * will return ERROR_IO_INCOMPLETE.
 *
 * On Windows NT, this function uses overlapped I/O, on Windows 9x, a blocking
 * write is performed in a background thread instead.
*/
DWORD pipe9x_write_initiate(PipeWriteHandle prh, const void *data, size_t data_size);

/**
 * @brief Get the result from a write operation.
 *
 * @param prw               PipeWriteHandle object to check status of.
 * @param data_written_out  Pointer to receive number of bytes written (on success).
 * @param wait              Whether to wait for completion before returning.
 *
 * This function gets the result of a write operation previous started using
 * the pipe9x_write_initiate() function.
 *
 * If the write completed successfully, ERROR_SUCCESS is returned and
 * *data_written_out is initialised with the number of bytes successfully
 * written to the pipe.
 *
 * If the write is still in progress and wait is FALSE, ERROR_IO_INCOMPLETE
 * will be returned.
 *
 * If any other error occurs, the relevant Win32 error code is returned.
*/
DWORD pipe9x_write_result(PipeWriteHandle pwh, size_t *data_written_out, BOOL wait);

/**
 * @brief Check if a write operation is pending.
 *
 * This function checks if a write operation is pending. It will return true
 * from the point pipe9x_write_initiate() is called until a call to
 * pipe9x_write_result() which does not return ERROR_IO_PENDING is made.
*/
BOOL pipe9x_write_pending(PipeWriteHandle pwh);

/**
 * @brief Get the underlying Windows HANDLE of the pipe.
 *
 * This function obtains the underlying Windows HANDLE of the pipe, this is
 * intended for passing one end of the pipe into another process or library.
 *
 * Once the handle has been passed into a child process or duplicated, the
 * original PipeWriteHandle object may be destroyed with pipe9x_write_close().
*/
HANDLE pipe9x_write_pipe(PipeWriteHandle pwh);

/**
 * @brief Get an event object for detecting I/O completion.
 *
 * This function obtains a HANDLE to an event object which can be used to
 * detect when a write operation has finished.
 *
 * The returned handle may be waited on, but must not be manually reset, set or
 * otherwise altered.
*/
HANDLE pipe9x_write_event(PipeWriteHandle prh);

#endif /* !PIPE9X_H */
