/*
Copyright (c) 2012 mingw-w64 project

Contributing author: Kai Tietz

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <windows.h>
/*
#include <dbghelp.h>
*/
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

int backtrace (void **buffer, int size);
char ** backtrace_symbols (void *const *buffer, int size);
void backtrace_symbols_fd (void *const *buffer, int size, int fd);

int
backtrace (void **buffer, int size)
{
  USHORT frames;

  HANDLE hProcess;
  if (size <= 0)
    return 0;
  hProcess = GetCurrentProcess ();
  frames = CaptureStackBackTrace (0, (DWORD) size, buffer, NULL);

  return (int) frames;
}

char **
backtrace_symbols (void *const *buffer, int size)
{
  size_t t = 0;
  char **r, *cur;
  int i;

  t = ((size_t) size) * ((sizeof (void *) * 2) + 6 + sizeof (char *));
  r = (char **) malloc (t);
  if (!r)
   return r;

  cur = (char *) &r[size];

  for (i = 0; i < size; i++)
    {
      r[i] = cur;
      sprintf (cur, "[+0x%Ix]", (size_t) buffer[i]);
      cur += strlen (cur) + 1;
    }

  return r;
}

void
backtrace_symbols_fd (void *const *buffer, int size, int fd)
{
  char s[128];
  int i;

  for (i = 0; i < size; i++)
    {
      sprintf (s, "[+0x%Ix]\n", (size_t) buffer[i]);
      write (fd, s, strlen (s));
    }
  _commit (fd);
}

/*
void printStack (void);

void printStack (void)
{
unsigned int i;
void *stack[100];
unsigned short frames;
SYMBOL_INFO *symbol;
HANDLE hProcess;

hProcess = GetCurrentProcess ();
SymInitialize (hProcess, NULL, TRUE);
frames = CaptureStackBackTrace( 0, 100, stack, NULL );
symbol = (SYMBOL_INFO *) calloc (sizeof (SYMBOL_INFO) + 256 * sizeof (char), 1);
symbol->MaxNameLen = 255;
symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
for (i = 0;i < frames; i++)
{
SymFromAddr (hProcess, (DWORD_PTR) (stack[i]), 0, symbol);
printf ("%u: %p %s = 0x%Ix\n", frames - i - 1, stack[i], symbol->Name, symbol->Address);
}

free (symbol);
}

int main ()
{
printStack ();
return 0;
}
*/

