#ifndef _PLURAL_EVAL_H
#define _PLURAL_EVAL_H

#include "plural-exp.h"

/* While the bison parser is able to support expressions of a maximum depth
   of YYMAXDEPTH = 10000, the runtime evaluation of a parsed plural expression
   has a smaller maximum recursion depth.
   If we did not limit the recursion depth, a program that just invokes
   ngettext() on a thread other than the main thread could get a crash by
   stack overflow in the following circumstances:
     - On systems with glibc, after the stack size has been reduced,
       e.g. on x86_64 systems after "ulimit -s 260".
       This stack size is only sufficient for ca. 3919 recursions.
       Cf. <https://unix.stackexchange.com/questions/620720/>
     - On systems with musl libc, because there the thread stack size (for a
       thread other than the main thread) by default is only 128 KB, see
       <https://wiki.musl-libc.org/functional-differences-from-glibc.html#Thread-stack-size>.
     - On AIX 7 systems, because there the thread stack size (for a thread
       other than the main thread) by default is only 96 KB, see
       <https://www.ibm.com/docs/en/aix/7.1?topic=programming-threads-library-options>.
       This stack size is only sufficient for between 887 and 1363 recursions,
       depending on the compiler and compiler optimization options.
   A maximum depth of 100 is a large enough for all practical needs
   and also small enough to avoid stack overflow even with small thread stack
   sizes.  */
#ifndef EVAL_MAXDEPTH
# define EVAL_MAXDEPTH 100
#endif

#define PLURAL_C_VAR  'n'
#define PLURAL_C_NOT  '!'
#define PLURAL_C_CMPL '~'
#define PLURAL_C_XOR  '^'
#define PLURAL_C_LOR  '|'
#define PLURAL_C_LAND '&'
#define PLURAL_C_MULT '*'
#define PLURAL_C_DIV  '/'
#define PLURAL_C_MOD  '%'
#define PLURAL_C_ADD  '+'
#define PLURAL_C_SUB  '-'
#define PLURAL_C_LT   '<'
#define PLURAL_C_GT   '>'
#define PLURAL_C_LE   '('
#define PLURAL_C_GE   ')'
#define PLURAL_C_EQ   '='
#define PLURAL_C_NE   '#'
#define PLURAL_C_QUEST '?'

int libnls_plural_eval(const struct expression *pexp, unsigned long int n);
int libnls_plural_eval_string(const char *exp, unsigned long int n);

#endif /* _PLURAL_EVAL_H */
