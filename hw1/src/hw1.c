#include <stdlib.h>

#include "debug.h"
#include "hw1.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the content of three frames of audio data and
 * two annotation fields have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 1 if validation succeeds and 0 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variables "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 1 if validation succeeds and 0 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int checkh(char *argv);
int checkud(char *argv);
int checkc(char *argv);
int checkp(char *argv);
int checkf(char *argv);
int checkfactor(char *argv);
int checkk(char *argv);
int checkkey(char *argv);
int checkStringValid(char *argv);


int validargs(int argc, char **argv)
{
    if(checkStringValid(*(argv+1))){

        if(checkh(*(argv+1)))   return 1;

        if(checkud(*(argv+1))){

            if(argc==2) return 1;   //-u/-d

            if(argc==3){    //-u/d -p
                if(checkStringValid(*(argv+2))
                    && checkp(*(argv+2)))   return 1;
            }

            if(argc==4){
                if(checkStringValid(*(argv+2))
                    && checkf(*(argv+2))
                    && checkfactor(*(argv+3)))   return 1;
            }
            if(argc==5){    //-u -p -f ...
                if(checkStringValid(*(argv+2))
                    && checkp(*(argv+2))
                    && checkStringValid(*(argv+3))
                    && checkf(*(argv+3))
                    && checkfactor(*(argv+4)))   return 1;
                            //-u -f ... -p
                if(checkStringValid(*(argv+2))
                    && checkf(*(argv+2))
                    && checkfactor(*(argv+3))
                    && checkStringValid(*(argv+4))
                    && checkp(*(argv+4)))   return 1;
            }
        }
        if(checkc(*(argv+1))){

            if(argc==4){
                if(checkStringValid(*(argv+2))
                    && checkk(*(argv+2))
                    && checkkey(*(argv+3))) return 1;
            }

            if(argc==5){
                if(checkStringValid(*(argv+2))
                    && checkk(*(argv+2))
                    && checkkey(*(argv+3))
                    && checkStringValid(*(argv+4))
                    && checkp(*(argv+4))) return 1;

                if(checkStringValid(*(argv+2))
                    && checkp(*(argv+2))
                    && checkStringValid(*(argv+3))
                    && checkk(*(argv+3))
                    && checkkey(*(argv+4))) return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief  Recodes a Sun audio (.au) format audio stream, reading the stream
 * from standard input and writing the recoded stream to standard output.
 * @details  This function reads a sequence of bytes from the standard
 * input and interprets it as digital audio according to the Sun audio
 * (.au) format.  A selected transformation (determined by the global variable
 * "global_options") is applied to the audio stream and the transformed stream
 * is written to the standard output, again according to Sun audio format.
 *
 * @param  argv  Command-line arguments, for constructing modified annotation.
 * @return 1 if the recoding completed successfully, 0 otherwise.
 */
int recode(char **argv) {
    AUDIO_HEADER au;
    if(!read_header(&au))   return 0;
    unsigned int size = au.data_offset -sizeof(au);
    if(size>ANNOTATION_MAX) return 0;
    if(!read_annotation(input_annotation, size))    return 0;
    unsigned int newSize=0;

    //check p (59th bit) and modify annotation
    if(!((global_options >> 59) & 1)){


        //get argv prepend
        for(int i=0;*(argv+i)!=NULL;i++){
            for(int j=0;*(*(argv+i)+j)!=0;j++){
                *(output_annotation+newSize)=*(*(argv+i)+j);
                newSize++;
                if(newSize>ANNOTATION_MAX)    return 0;
            }
            *(output_annotation+newSize)=' ';
            newSize++;
        }
        *(output_annotation+newSize-1)='\n';
        //set data_offset

        for(int i=0;i<size;i++){
            *(output_annotation+newSize+i)=*(input_annotation+i);
            if((newSize)>ANNOTATION_MAX)    return 0;
        }
        while(newSize%8!=0) newSize++;
        if(newSize>ANNOTATION_MAX)    return 0;
        au.data_offset+=newSize;
    }




    int t=0;
    int *frame=&t;
    *(frame+1)=0;


    //get a lot of shit from global_options
    int factor = (global_options>>48) - ((global_options>>58)<<10)+1;
    unsigned long key = global_options-((global_options>>32)<<32);
    mysrand(key);
    if((global_options >> 62) & 1)
        au.data_size = ((au.data_size/((au.encoding-1)*au.channels)-1)/factor+1)*((au.encoding-1)*au.channels);
    else if((global_options >> 61) & 1)
        au.data_size = ((au.data_size/((au.encoding-1)*au.channels)-1)*factor+1)*((au.encoding-1)*au.channels);
    int annotationSize = au.data_offset -sizeof(au);

    //write header and write annotation
    if(!write_header(&au))  return 0;
    if(!write_annotation(output_annotation,annotationSize)) return 0;


    for(int i=0;read_frame(frame, au.channels, au.encoding-1);i++){


        //set previous frame
        for(int j=0;j<au.channels*(au.encoding-1);j++){
            *(previous_frame+j)=*(input_frame+j);
        }

        //convert frame to input_frame
        for(int j=0,index=0;j<au.channels;j++){
            for(int k=au.encoding-1;k>0;k--,index++){
                *(input_frame+index)=(*(frame+j)>>(8*k-8))-((*(frame+j)>>(8*k))<<8);
            }
        }

        //check if speed up
        if((global_options >> 62) & 1){
            //set out frame
            if(i%factor==0){
                for(int j=0;j<au.channels*(au.encoding-1);j++)
                    *(output_frame+j)=*(input_frame+j);

                //convert out put frame to frame
                for(int j=0;j<au.channels;j++){
                    for(int k=0,index=0;k<au.encoding-1;k++,index++){
                        *(frame+j)=(*(frame+j)<<8)+*(unsigned char *)(output_frame+index);
                    }
                }

                if(!write_frame(frame,au.channels, au.encoding-1))  return 0;
            }
        }

        //check if slow down
        else if((global_options >> 61) & 1){
            if(i==0){
                for(int l=0,index=0;l<au.channels;l++){
                    int curr=0;

                    for(int k=0;k<au.encoding-1;k++,index++){
                        curr=(curr<<8)+*(input_frame+index);
                    }

                    *(frame+l)=curr;

                }
                if(!write_frame(frame,au.channels, au.encoding-1))  return 0;

                *frame=0;
                *(frame+1)=0;
                continue;
            }


            for(int j=1;j<=factor;j++){
                *frame=0;
                *(frame+1)=0;

                for(int l=0,index=0;l<au.channels;l++){
                    int pre=0,curr=0;

                    for(int k=0;k<au.encoding-1;k++,index++){
                        pre=(pre<<8)+*(previous_frame+index);
                        curr=(curr<<8)+*(input_frame+index);
                    }

                    *(frame+l)=pre+(curr-pre)*j/factor;

                }
                if(!write_frame(frame,au.channels, au.encoding-1))  return 0;
            }
        }


        //check if crypt
        if((global_options >> 60) & 1){
            for(int k=0;k<au.channels;k++){
                *(frame+k)=*(frame+k)^myrand32();

            }
            if(!write_frame(frame,au.channels, au.encoding-1))  return 0;
        }

        *frame=0;
        *(frame+1)=0;
    }

    return 1;
}

int read_header(AUDIO_HEADER *hp){

    int bit = 32;
    unsigned int a;
    hp->magic_number=0;

    //check magic number
    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->magic_number += a << bit;
    }
    if(hp->magic_number != AUDIO_MAGIC) return 0;

    //check data off set
    bit = 32;
    hp->data_offset=0;

    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->data_offset += a << bit;
    }
    if(hp->data_offset%8!=0)    return 0;

    //store data size
    bit = 32;
    hp->data_size=0;

    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->data_size += a << bit;
    }
    if(hp->data_size<=0)    return 0;

    //check encoding
    bit = 32;
    hp->encoding=0;

    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->encoding += a << bit;
    }
    if(hp->encoding!=PCM8_ENCODING &&\
        hp->encoding!=PCM16_ENCODING &&\
        hp->encoding!=PCM24_ENCODING &&\
        hp->encoding!=PCM32_ENCODING)    return 0;

    //store sample rate
    bit = 32;
    hp->sample_rate=0;
    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->sample_rate += a << bit;
    }

    //check channel;
    bit = 32;
    hp->channels=0;
    while(bit>0){
        if((a=getchar())==EOF)  return 0;
        bit-=8;
        hp->channels += a << bit;
    }
    if(hp->channels!=1 && hp->channels!=2)  return 0;
    //printf("%lx\n", sizeof(AUDIO_HEADER));
    return 1;
}

int write_header(AUDIO_HEADER *hp){
    //write  magic number
    if(stdin==NULL || stdout==NULL) return 0;
    unsigned int a = hp->magic_number>>24;  //2e
    if(a!=putchar(a))   return 0;   //.
    a = a<<8;
    a = (hp->magic_number>>16) - a; //2e73-2e00
    if(a!=putchar(a))   return 0;   //s
    a = hp->magic_number>>16;       //2e73
    a = a<<8;
    a = (hp->magic_number>>8) - a;  //2e736e-2e7300
    if(a!=putchar(a))   return 0;   //6e
    a = hp->magic_number>>8;
    a = a<<8;
    a = (hp->magic_number>>0) - a;
    if(a!=putchar(a))   return 0;

    // write data offset
    a = hp->data_offset>>24;
    if(a!=putchar(a))   return 0;
    a = a<<8;
    a = (hp->data_offset>>16) - a;
    if(a!=putchar(a))   return 0;
    a = hp->data_offset>>16;
    a = a<<8;
    a = (hp->data_offset>>8) - a;
    if(a!=putchar(a))   return 0;
    a = hp->data_offset>>8;
    a = a<<8;
    a = (hp->data_offset>>0) - a;
    if(a!=putchar(a))   return 0;

    //write data size
    a = hp->data_size>>24;
    if(a!=putchar(a))   return 0;
    a = a<<8;
    a = (hp->data_size>>16) - a;
    if(a!=putchar(a))   return 0;
    a = hp->data_size>>16;
    a = a<<8;
    a = (hp->data_size>>8) - a;
    if(a!=putchar(a))   return 0;
    a = hp->data_size>>8;
    a = a<<8;
    a = (hp->data_size>>0) - a;
    if(a!=putchar(a))   return 0;

    //write encoding
    a = hp->encoding>>24;
    if(a!=putchar(a))   return 0;
    a = a<<8;
    a = (hp->encoding>>16) - a;
    if(a!=putchar(a))   return 0;
    a = hp->encoding>>16;
    a = a<<8;
    a = (hp->encoding>>8) - a;
    if(a!=putchar(a))   return 0;
    a = hp->encoding>>8;
    a = a<<8;
    a = (hp->encoding>>0) - a;
    if(a!=putchar(a))   return 0;

    //write sample rate
    a = hp->sample_rate>>24;
    if(a!=putchar(a))   return 0;
    a = a<<8;
    a = (hp->sample_rate>>16) - a;
    if(a!=putchar(a))   return 0;
    a = hp->sample_rate>>16;
    a = a<<8;
    a = (hp->sample_rate>>8) - a;
    if(a!=putchar(a))   return 0;
    a = hp->sample_rate>>8;
    a = a<<8;
    a = (hp->sample_rate>>0) - a;
    if(a!=putchar(a))   return 0;

    //write channels
    a = hp->channels>>24;
    if(a!=putchar(a))   return 0;
    a = a<<8;
    a = (hp->channels>>16) - a;
    if(a!=putchar(a))   return 0;
    a = hp->channels>>16;
    a = a<<8;
    a = (hp->channels>>8) - a;
    if(a!=putchar(a))   return 0;
    a = hp->channels>>8;
    a = a<<8;
    a = (hp->channels>>0) - a;
    if(a!=putchar(a))   return 0;

    return 1;
}

int read_annotation(char *ap, unsigned int size){
    if(stdin==NULL) return 0;
    if(size==0){
        *ap = '\0';
        return 1;
    }
    for(int i=0;i<size;i++){
        if((*(ap+i)=getchar())==EOF)    return 0;
    }
    return 1;
}

int write_annotation(char *ap, unsigned int size){
    if(stdin==NULL || stdout==NULL) return 0;
    for(int i=0; i<size; i++){
        if(putchar(*(ap+i))!=*(ap+i))   return 0;
    }
    return 1;
}

int read_frame(int *fp, int channels, int bytes_per_sample){
    //load frame to fp
    if(stdin==NULL) return 0;

    int byte;
    for(int h=0;h<channels;h++){
        for(int i=0;i<bytes_per_sample;i++){
            if((byte=getchar())==EOF){

               fp=NULL;
                return 0;
            }
            //debug("%d\n",byte);
            *(fp+h)=(*(fp+h)<<8)+byte;
        }
    }
    return 1;
}

int write_frame(int *fp, int channels, int bytes_per_sample){
    if(stdin==NULL || stdout==NULL) return 0;
    for(int j=0;j<channels;j++){
        for(int k=bytes_per_sample;k>0;k--){
            putchar((*(fp+j)>>(8*k-8))-((*(fp+j)>>(8*k))<<8));
        }
    }
    return 1;
}

int checkStringValid(char *argv){
    if(*argv=='-' && *(argv+2)==0)  {
        return 1;
    }
    return 0;
}

int checkh(char *argv){
    if(*(argv+1)!='h')  return 0;
    global_options = 1UL << 63;
    return 1;
}

int checkud(char *argv){
    if(*(argv+1)!='u' && *(argv+1)!='d')    return 0;
    if(*(argv+1)=='u')  global_options = 1UL << 62;
    else if(*(argv+1)=='d') global_options = 1UL << 61;
    return 1;
}

int checkc(char *argv){
    if(*(argv+1)!='c')  return 0;
    global_options = 1UL << 60;
    return 1;
}

int checkp(char *argv){
    if(*(argv+1)!='p')  return 0;
    global_options += 1UL << 59;
    return 1;
}

int checkf(char *argv){
    if(*(argv+1)!='f')  return 0;
    return 1;
}

int checkfactor(char *argv){
    unsigned long total=0;
    for(int i=0;*(argv+i)!=0;i++){
        if(*(argv+i)<48 || *(argv+i)>57)  return 0;
        total = total*10+*(argv+i)-48;
    }
    if(total<1 || total>1024)   return 0;
    global_options += (total-1)<<48;
    return 1;
}

int checkk(char *argv){
    if(*(argv+1)!='k')  return 0;
    return 1;
}


int checkkey(char *argv){
    //count amount of key
    int nkey=0;
    for(int i=0;*(argv+i)!=0;i++){
        nkey++;
    }

    unsigned long total=0;
    for(int i=0;*(argv+i)!=0;i++,nkey--){
        if(*(argv+i)<48 || *(argv+i)>102 || (*(argv+i)>70 && *(argv+i)<97) || (*(argv+i)>57 && *(argv+i)<65)) return 0;
            //set k to decimal
            char k=*(argv+i);
            if(*(argv+i)>47 && *(argv+i)<58)    k-=48;
            if(*(argv+i)>64 && *(argv+i)<71)    k-=55;
            if(*(argv+i)>96 && *(argv+i)<103)   k-=87;
            total=total*16+k;
            //global_options += k <<(nkey-1)*4;

    }
        global_options += total <<0;
        //printf("%lx\n",global_options>>60);

    return 1;
}



