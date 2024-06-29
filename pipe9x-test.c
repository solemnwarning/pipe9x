/* Pipe9x - Anonymous pipes with overlapped I/O semantics on Windows 9x
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

#include <stdio.h>

#include "pipe9x.h"

#define ASSERT_TRUE(expr, msg) \
	if(expr) \
	{ \
		fprintf(stderr, "PASS: %s\n", msg); \
	} \
	else{ \
		fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
		return ++num_failures; \
	}

#define EXPECT_TRUE(expr, msg) \
	if(expr) \
	{ \
		fprintf(stderr, "PASS: %s\n", msg); \
	} \
	else{ \
		fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
		++num_failures; \
	}

int main()
{
	int num_failures = 0;
	
	/* Set up a pipe. */
	
	PipeReadHandle prh;
	PipeWriteHandle pwh;
	
	ASSERT_TRUE(pipe9x_create(&prh, (128 * 1024), NULL, &pwh, (128 * 1024), NULL) == ERROR_SUCCESS,
		"pipe9x_create() returns ERROR_SUCCESS");
	
	ASSERT_TRUE(prh != NULL, "pipe9x_create() initialises a PipeReadHandle");
	ASSERT_TRUE(pwh != NULL, "pipe9x_create() initialises a PipeWriteHandle");
	
	/* Verify the initial state of the pipe and handles. */
	
	void *data;
	size_t data_size;
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_read_event(prh), 0) == WAIT_OBJECT_0),
		"PipeReadHandle event object is signalled after construction");
	
	EXPECT_TRUE((pipe9x_read_pending(prh) == FALSE),
		"PipeReadHandle has no read pending after construction");
	
	EXPECT_TRUE(pipe9x_read_result(prh, &data, &data_size, FALSE) == ERROR_INVALID_PARAMETER,
		"pipe9x_read_result() returns ERROR_INVALID_PARAMETER when no read is pending");
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_write_event(pwh), 0) == WAIT_OBJECT_0),
		"PipeWriteHandle event object is initially signalled");
	
	EXPECT_TRUE((pipe9x_write_pending(pwh) == FALSE),
		"PipeWriteHandle has no write pending after construction");
	
	EXPECT_TRUE(pipe9x_write_result(pwh, &data_size, FALSE) == ERROR_INVALID_PARAMETER,
		"pipe9x_write_result() returns ERROR_INVALID_PARAMETER when no write is pending");
	
	/* Try reading data from the empty pipe. */
	
	EXPECT_TRUE(pipe9x_read_initiate(prh) == ERROR_IO_PENDING,
		"pipe9x_read_initiate() can initiate a read after construction");
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_read_event(prh), 0) == WAIT_TIMEOUT),
		"PipeReadHandle event object is unsignalled after initiating a read");
	
	EXPECT_TRUE((pipe9x_read_pending(prh) == TRUE),
		"PipeReadHandle has a read pending after initiating a read");
	
	EXPECT_TRUE(pipe9x_read_result(prh, &data, &data_size, FALSE) == ERROR_IO_INCOMPLETE,
		"pipe9x_read_result() returns ERROR_IO_INCOMPLETE when read is incomplete");
	
	EXPECT_TRUE(pipe9x_read_initiate(prh) == ERROR_IO_INCOMPLETE,
		"pipe9x_read_initiate() returns ERROR_IO_INCOMPLETE when read is already pending");
	
	/* Verify write handle is unaffected. */
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_write_event(pwh), 0) == WAIT_OBJECT_0),
		"PipeWriteHandle event object remains signalled after initiating read");
	
	EXPECT_TRUE((pipe9x_write_pending(pwh) == FALSE),
		"PipeWriteHandle has no write pending after initiating read");
	
	EXPECT_TRUE(pipe9x_write_result(pwh, &data_size, FALSE) == ERROR_INVALID_PARAMETER,
		"pipe9x_write_result() returns ERROR_INVALID_PARAMETER when no write is pending");
	
	/* Write some data to the pipe. */
	
	{
		char data[64];
		memset(data, 0xFF, sizeof(data));
		
		EXPECT_TRUE(pipe9x_write_initiate(pwh, data, sizeof(data)) == ERROR_IO_PENDING,
			"pipe9x_write_initiate() can initiate a write after construction");
		
		EXPECT_TRUE(pipe9x_write_initiate(pwh, data, sizeof(data)) == ERROR_IO_INCOMPLETE,
			"pipe9x_write_initiate() returns ERROR_IO_INCOMPLETE when write is already pending");
	}
	
	EXPECT_TRUE((pipe9x_write_pending(pwh) == TRUE),
		"PipeWriteHandle has write pending after initiating write");
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_write_event(pwh), 1000) == WAIT_OBJECT_0),
		"PipeWriteHandle event object is signalled when write completes");
	
	EXPECT_TRUE(pipe9x_write_result(pwh, &data_size, TRUE) == ERROR_SUCCESS,
		"pipe9x_write_result() returns ERROR_SUCCESS when write completes");
	
	EXPECT_TRUE(data_size == 64,
		"pipe9x_write_result() returns expected size when write completes");
	
	EXPECT_TRUE((pipe9x_write_pending(pwh) == FALSE),
		"PipeWriteHandle has no write pending after write result is handled");
	
	/* Try to read the data from the pipe. */
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_read_event(prh), 1000) == WAIT_OBJECT_0),
		"PipeReadHandle event object is signalled after data is written to pipe");
	
	EXPECT_TRUE((pipe9x_read_pending(prh) == TRUE),
		"PipeReadHandle has a read pending when result has not been handled");
	
	EXPECT_TRUE(pipe9x_read_result(prh, &data, &data_size, TRUE) == ERROR_SUCCESS,
		"pipe9x_read_result() returns ERROR_SUCCESS when read completes");
	
	EXPECT_TRUE((pipe9x_read_pending(prh) == FALSE),
		"PipeReadHandle has no read pending when has result been handled");
	
	EXPECT_TRUE((WaitForSingleObject(pipe9x_read_event(prh), 0) == WAIT_OBJECT_0),
		"PipeReadHandle event object remains signalled when result has been handled");
	
	{
		char expect_data[64];
		memset(expect_data, 0xFF, sizeof(expect_data));
		
		EXPECT_TRUE(data_size == 64 && memcmp(data, expect_data, 64) == 0,
			"PipeReadHandle returns expected data");
	}
	
	/* Fill the pipe up.
	 *
	 * The buffer size we provide is only advisory, so we just keep pumping
	 * data in until it won't take any more (or it gets absurd).
	*/
	
	size_t total_data_written = 0;
	const size_t MAX_DATA_COMMIT = 64 * 1024 * 1024; /* 64MiB */
	
	static char big_data[8192];
	memset(big_data, 0xDD, sizeof(big_data));
	
	BOOL pipe_filled_ok = TRUE;
	
	while(TRUE && total_data_written < MAX_DATA_COMMIT)
	{
		if(pipe9x_write_initiate(pwh, big_data, sizeof(big_data)) != ERROR_IO_PENDING)
		{
			fprintf(stderr, "Unexpected failure from pipe9x_write_initiate()\n");
			pipe_filled_ok = FALSE;
			
			break;
		}
		
		if(WaitForSingleObject(pipe9x_write_event(pwh), 5000) == WAIT_TIMEOUT)
		{
			/* Writing has stalled, hopefully the pipe is full now. */
			break;
		}
		else{
			if(pipe9x_write_result(pwh, &data_size, TRUE) != ERROR_SUCCESS)
			{
				fprintf(stderr, "Unexpected failure from pipe9x_write_result()\n");
				pipe_filled_ok = FALSE;
				
				break;
			}
			
			total_data_written += data_size;
		}
	}
	
	ASSERT_TRUE(pipe_filled_ok, "PipeWriteHandle write stalls when pipe is filled");
	
	/* Start reading the data out of the pipe. */
	
	size_t total_data_read = 0;
	
	while(TRUE && total_data_read < total_data_written)
	{
		if(pipe9x_read_initiate(prh) != ERROR_IO_PENDING)
		{
			fprintf(stderr, "Unexpected failure from pipe9x_read_initiate()\n");
			++num_failures;
			
			break;
		}
		
		if(WaitForSingleObject(pipe9x_read_event(prh), 5000) == WAIT_TIMEOUT)
		{
			/* Reading has stalled... is that everything? */
			break;
		}
		else{
			if(pipe9x_read_result(prh, &data, &data_size, TRUE) != ERROR_SUCCESS)
			{
				fprintf(stderr, "Unexpected failure from pipe9x_read_result()\n");
				++num_failures;
				
				break;
			}
			
			total_data_read += data_size;
		}
	}
	
	/* The write should have completed now. */
	
	EXPECT_TRUE(WaitForSingleObject(pipe9x_write_event(pwh), 1000) == WAIT_OBJECT_0,
		"PipeWriteHandle event object is signalled when write completes");
	
	EXPECT_TRUE(pipe9x_write_result(pwh, &data_size, TRUE) == ERROR_SUCCESS,
		"pipe9x_write_result() returns ERROR_SUCCESS when write completes");
	
	total_data_written += data_size;
	
	/* Write one more bit of data and then close the write handle... */
	
	EXPECT_TRUE(pipe9x_write_initiate(pwh, big_data, sizeof(big_data)) == ERROR_IO_PENDING,
		"pipe9x_write_initiate() can initiate a write after another has finished");
	
	EXPECT_TRUE(pipe9x_write_result(pwh, &data_size, TRUE) == ERROR_SUCCESS,
		"pipe9x_write_result() returns ERROR_SUCCESS when write completes");
	
	EXPECT_TRUE(data_size == sizeof(big_data),
		"pipe9x_write_result() returns correct write size");
	
	total_data_written += data_size;
	
	pipe9x_write_close(pwh);
	pwh = NULL;
	
	/* ...and read it from the still-open read handle... */
	
	ASSERT_TRUE(pipe9x_read_initiate(prh) == ERROR_IO_PENDING,
		"pipe9x_read_initiate() can initiate a read after another has finished");
	
	EXPECT_TRUE(WaitForSingleObject(pipe9x_read_event(prh), 1000) == WAIT_OBJECT_0,
		"PipeReadHandle event object is signalled when write end of pipe is closed");
	
	ASSERT_TRUE(pipe9x_read_result(prh, &data, &data_size, TRUE) == ERROR_SUCCESS,
		"pipe9x_read_result() returns ERROR_SUCCESS when read from a pipe with a closed write handle completes");
	
	total_data_read += data_size;
	
	/* ...and now the next read should fail with ERROR_BROKEN_PIPE. */
	
	ASSERT_TRUE(pipe9x_read_initiate(prh) == ERROR_IO_PENDING,
		"pipe9x_read_initiate() can initiate a read after another has finished");
	
	EXPECT_TRUE(WaitForSingleObject(pipe9x_read_event(prh), 1000) == WAIT_OBJECT_0,
		"PipeReadHandle event object is signalled when write end of pipe is closed");
	
	ASSERT_TRUE(pipe9x_read_result(prh, &data, &data_size, TRUE) == ERROR_BROKEN_PIPE,
		"pipe9x_read_result() returns ERROR_BROKEN_PIPE when there is no data in a pipe with a closed write handle");
	
	EXPECT_TRUE(total_data_written == total_data_read, "No data is lost when pipe is filled");
	
	if(num_failures == 0)
	{
		fprintf(stderr, "\nAll tests passed!\n");
	}
	
	return num_failures;
}
