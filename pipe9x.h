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

#define PIPE_READ_SIZE 32768

typedef struct _PipeWriteHandle *PipeWriteHandle;
typedef struct _PipeReadHandle *PipeReadHandle;

DWORD pipe9x_create(PipeReadHandle *prh_out, LPSECURITY_ATTRIBUTES pr_security, PipeWriteHandle *pwh_out, LPSECURITY_ATTRIBUTES pw_security);

void pipe9x_read_close(PipeReadHandle prh);

DWORD pipe9x_read_initiate(PipeReadHandle prh);

DWORD pipe9x_read_result(PipeReadHandle prh, void **data_out, size_t *data_size_out, BOOL wait);

BOOL pipe9x_read_pending(PipeReadHandle prh);

HANDLE pipe9x_read_pipe(PipeReadHandle prh);

HANDLE pipe9x_read_event(PipeReadHandle prh);

void pipe9x_write_close(PipeWriteHandle pwh);

BOOL pipe9x_write_pending(PipeWriteHandle pwh);

HANDLE pipe9x_write_pipe(PipeWriteHandle pwh);

HANDLE pipe9x_write_event(PipeWriteHandle prh);

#endif /* !PIPE9X_H */
