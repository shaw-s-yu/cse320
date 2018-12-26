/*
 * Error handling routines
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

int errors;
int warnings;
int dbflag = 1;

void fatal(char *fmt, ...)
{
        va_list va_Argv;
        va_start(va_Argv, fmt);
        fprintf(stderr, "\nFatal error: ");
        vfprintf(stderr, fmt, va_Argv);
        fprintf(stderr, "\n");
        exit(1);
}

void error(char *fmt, ...)
{
        va_list va_Argv;
        va_start(va_Argv, fmt);
        fprintf(stderr, "\nError: ");
        vfprintf(stderr, fmt, va_Argv);
        fprintf(stderr, "\n");
        errors++;
}

void warning(char *fmt, ...)
{
        va_list va_Argv;
        va_start(va_Argv, fmt);
        fprintf(stderr, "\nWarning: ");
        vfprintf(stderr, fmt, va_Argv);
        fprintf(stderr, "\n");
        va_end(va_Argv);
        warnings++;
}

void debug(char *fmt, ...)
{
        if(!dbflag) return;
        va_list va_Argv;
        va_start(va_Argv, fmt);
        fprintf(stderr, "\nDebug: ");
        vfprintf(stderr, fmt, va_Argv);
        fprintf(stderr, "\n");
}
