// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

// obtained from http://panthema.net/2008/0901-stacktrace-demangled/ on 24 December 2014 by Ben Haller
// I have made various changes to formatting, as well as to make it work better on OS X.

#ifndef _STACKTRACE_H_
#define _STACKTRACE_H_


#include <stdio.h>


// Print a demangled stack backtrace of the caller function to FILE* out.
void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63);


#endif // _STACKTRACE_H_




