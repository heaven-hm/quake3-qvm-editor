#include "bg_lib.h"

// Forward declarations
void printf(const char* fmt, ...);
int fibonacci(int n);

/*
================
vmMain

The entry-point for Quake III QVM bytecode execution.
Control passes into this function when the engine calls the VM.
================
*/
int vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4,
           int arg5, int arg6, int arg7, int arg8, int arg9, int arg10,
           int arg11)
{
    if (command == 0) {
        // Command 0: VM Init
        printf("^2[QVM Editor]^7 Module loaded successfully!\n");
        printf("^3[QVM Editor]^7 fibonacci(7) = %i\n", fibonacci(7));
    } else {
        printf("^1[QVM Editor]^7 Unknown engine command: %i\n", command);
    }

    return 0;
}

void printf(const char* fmt, ...)
{
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);

    // trap_Print is available in the qagame, cgame and ui syscall tables.
    // (trap_Printf only exists for qagame, so trap_Print keeps this template
    //  linkable against whichever module is selected.)
    trap_Print(text);
}

int fibonacci(int n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}
