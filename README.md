# ![Pipe9x logo](pipe9x.png) Pipe9x

This library provides Windows pipes with asyncronous I/O on Windows 9x and Windows NT, notifying of read/write completion via event objects and/or polling.

On Windows 98, the library uses [anonymous pipes](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe) and background threads to run the ReadFile/WriteFile calls.

On Windows NT, the library uses named pipes and overlapped I/O.

The API is documented with Doxygen and [readable online](https://solemnwarning.github.io/pipe9x/pipe9x_8h.html).
