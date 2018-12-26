#include <stdlib.h>

#include "hw1.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{

    int *a=(int *)input_frame;
    input_frame[0]=0;
    input_frame[1]=1;
    input_frame[2]=2;
    input_frame[3]=3;
    input_frame[4]=4;
    input_frame[5]=5;
    input_frame[6]=6;
    input_frame[7]=7;
    debug("a:%x %x\n",*a,*(a+1));
    if(!validargs(argc, argv)){
        //printf("Setting: 0x%lX\n", global_options);
        USAGE(*argv, EXIT_FAILURE);
    }
    //printf("Setting: 0x%lX\n", global_options);

    recode(argv);
    debug("Options: 0x%lX", global_options);
    if(global_options & (0x1L << 63)) {
        USAGE(*argv, EXIT_SUCCESS);
    }

    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
