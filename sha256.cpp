// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         SHA256 calculation
// ****************************************************************************

// Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017,
// 2018, 2019, 2020
// Guy Voncken
//
// This file is part of Guymager.
//
// Guymager is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Guymager is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Guymager. If not, see <http://www.gnu.org/licenses/>.

// This module was initially based on the file
//    www.spale.com/download/scrypt/scrypt1.0/sha256.c
// copyrighted by Christophe Devine, with assembler i386/amd64-optimisation
// and big endian debugging added by Guy Voncken. That optimised code
// was faster than the Linux' standard tool sha256sum. You find it in
// this module under SHA256_OLD macro.
//
// In later tests it was observed that sha256sum (or gcc) must have been
// improved; sha256sum was the fastet now. That's why this module nowadays
// is based on sha256sum's code (see Debian package coreutils, for example).
// The core functions have been mainly copy/pasted; they have been written
// by David Madore and Scott G. Miller and are copyrighted by the Free
// Software Foundation, Inc.


//#define SHA256_OLD

#include <stddef.h>
#include <stdint.h>
#include "sha256.h"

#ifdef SHA256_OLD

#include <string.h>
#include <netinet/in.h>


#if defined(__i386__) || defined (__amd64__)
   #define SHA256_USE_OPTIMISED_ASSEMBLER_CODE
#endif

// Guy: The original GET_UINT32 and PUT_UINT32 macros are
//   - too slow
//   - not working on powerpc
// I replaced them by the powerful htonl/ntohl functions.

/*
#define GET_UINT32(n,b,i)                       \
{                                               \
    (n) = ( (uint32) (b)[(i)    ] << 24 )       \
        | ( (uint32) (b)[(i) + 1] << 16 )       \
        | ( (uint32) (b)[(i) + 2] <<  8 )       \
        | ( (uint32) (b)[(i) + 3]       );      \
}

#define PUT_UINT32(n,b,i)                       \
{                                               \
    (b)[(i)    ] = (uint8) ( (n) >> 24 );       \
    (b)[(i) + 1] = (uint8) ( (n) >> 16 );       \
    (b)[(i) + 2] = (uint8) ( (n) >>  8 );       \
    (b)[(i) + 3] = (uint8) ( (n)       );       \
}
*/

#define GET_UINT32(n,b,i)                       \
   (n)= htonl (*((uint32 *)&(b)[i]));

#define PUT_UINT32(n,b,i)                       \
   *((uint32 *)&(b)[i]) = ntohl((n));

void SHA256Init (t_pSHA256Context pContext)
{
    pContext->total[0] = 0;
    pContext->total[1] = 0;

    pContext->state[0] = 0x6A09E667;
    pContext->state[1] = 0xBB67AE85;
    pContext->state[2] = 0x3C6EF372;
    pContext->state[3] = 0xA54FF53A;
    pContext->state[4] = 0x510E527F;
    pContext->state[5] = 0x9B05688C;
    pContext->state[6] = 0x1F83D9AB;
    pContext->state[7] = 0x5BE0CD19;
}

#ifdef SHA256_USE_OPTIMISED_ASSEMBLER_CODE
   #define P(a,b,c,d,e,f,g,h,x,K)                                                                       \
       __asm__  __volatile__ (                                                                          \
                /* ------------------------------------------------------------------------------- */   \
                /* h + S3(e)                                    +   F1(e,f,g)             + K + x  */   \
                /* h + (ROTR(e, 6) ^ ROTR(e,11) ^ ROTR(e,25))   +   (g ^ (e & (f ^ g)))   + K + x  */   \
                /*           $5            $5           $5           $7   $5   $6  $7       $9  $8 */   \
                /* ------------------------------------------------------------------------------- */   \
                                                                                                        \
                "movl %5,%%ebx;"                                      \
                "movl %%ebx,%%edx;"                                   \
                "rorl $6,%%ebx;"                                      \
                "movl %%ebx,%%eax;"                                   \
                "rorl $5,%%eax;"                                      \
                "xorl %%ebx,%%eax;"                                   \
                "rorl $19,%%ebx;"                                     \
                "xorl %%ebx,%%eax;"  /* eax = S3(); edx = e */        \
                                                                      \
                "movl %7,%%ebx;"                                      \
                "movl %%ebx,%%ecx;"                                   \
                "xorl %6,%%ecx;"                                      \
                "andl %%edx,%%ecx;"                                   \
                "xorl %%ebx,%%ecx;"   /* ecx = F1() */                \
                                                                      \
                "leal " #K "(%%ecx,%%eax,1),%%eax;"                   \
                "addl %1,%%eax;"                                      \
                "addl %8,%%eax;"                                      \
                "addl %%eax, %0;"   /*  d += temp1; */                \
                                                                      \
                /* ----------------------------------------------------------------------- */   \
                /* S2(a)                                    +   F0(a,b,c);                 */   \
                /* (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))   +   ((a & b) | (c & (a | b)))  */   \
                /* ----------------------------------------------------------------------- */   \
                                                                      \
                "movl %2,%%ebx;"                                      \
                "movl %3,%%ecx;"                                      \
                "movl %%ecx,%%edx;"                                   \
                "and  %%ebx,%%ecx;"  /* ecx = (a & b) */              \
                "or   %%ebx,%%edx;"  /* edx = (a | b) */              \
                "and  %4,%%edx;"                                      \
                "or   %%edx,%%ecx;"  /* ecx = F0(); ebx = a */        \
                                                                      \
                "rorl $2,%%ebx;"                                      \
                "movl %%ebx,%%edx;"                                   \
                "rorl $11,%%ebx;"                                     \
                "xorl %%ebx,%%edx;"                                   \
                "rorl $9,%%ebx;"                                      \
                "xorl %%ebx,%%edx;"  /* edx = S2() */                 \
                                                                      \
                "addl %%edx,%%ecx;"  /* ecx = Resultat */             \
                                                                      \
                "addl %%eax,%%ecx;"  /* h = temp1 + temp2; */         \
                "movl %%ecx, %1;"                                     \
            :"=m"(d), "=m"(h)                                                                   \
            :"m"(a), "m"(b), "m"(c), "m"(e), "m"(f), "m"(g), "m"(x), "i"(K), "0"(d), "1"(h)     \
            /*   2       3       4       5       6       7       8       9       0       1 */   \
                                                                                                \
            :"%eax", "%ebx", "%ecx", "%edx", "%cc", "memory")

   #if defined(__i386__)
      #define PR(a,b,c,d,e,f,g,h,i,K)                                         \
          __asm__  __volatile__ (                                             \
                                                                              \
                   /* ----------------------------------- */                  \
                   /* W[t] = S1(W[t -  2]) + W[t -  7] +  */                  \
                   /*        S0(W[t - 15]) + W[t - 16]    */                  \
                   /* ----------------------------------- */                  \
                                                                              \
                   "movl %10,%%edx;"                                          \
                   "movl 4*(" #i "- 7)(%%edx),%%ecx;"  /* ecx used for sum */ \
                   "addl 4*(" #i "-16)(%%edx),%%ecx;"                         \
                                                                              \
                   "movl 4*(" #i "- 2)(%%edx),%%ebx;"                         \
                   "movl %%ebx,%%eax;"                                        \
                   "shrl $10,%%eax;"                                          \
                   "rorl $17,%%ebx;"                                          \
                   "xorl %%ebx,%%eax;"                                        \
                   "rorl $2,%%ebx;"                                           \
                   "xorl %%ebx,%%eax;"  /* eax = S1() */                      \
                   "addl %%eax,%%ecx;"                                        \
                                                                              \
                   "movl 4*(" #i "-15)(%%edx),%%ebx;"                         \
                   "movl %%ebx,%%eax;"                                        \
                   "shrl $3,%%eax;"                                           \
                   "rorl $7,%%ebx;"                                           \
                   "xorl %%ebx,%%eax;"                                        \
                   "rorl $11,%%ebx;"                                          \
                   "xorl %%ebx,%%eax;"  /* eax = S1() */                      \
                   "addl %%eax,%%ecx;"                                        \
                                                                              \
                   "movl %%ecx, 4*(" #i ")(%%edx);" /* Write result to W[t], keep ecx for later */         \
                                                                                                           \
                   /* ------------------------------------------------------------------------------- */   \
                   /* h + S3(e)                                    +   F1(e,f,g)             + K + x  */   \
                   /* h + (ROTR(e, 6) ^ ROTR(e,11) ^ ROTR(e,25))   +   (g ^ (e & (f ^ g)))   + K + x  */   \
                   /*           $5            $5           $5           $7   $5   $6  $7       $9  $8 */   \
                   /* ------------------------------------------------------------------------------- */   \
                                                                                                           \
                   "movl %5,%%ebx;"                                      \
                   "movl %%ebx,%%edx;"                                   \
                   "rorl $6,%%ebx;"                                      \
                   "movl %%ebx,%%eax;"                                   \
                   "rorl $5,%%eax;"                                      \
                   "xorl %%ebx,%%eax;"                                   \
                   "rorl $19,%%ebx;"                                     \
                   "xorl %%ebx,%%eax;"  /* eax = S3(); edx = e */        \
                   "addl %%ecx,%%eax;"  /* Add R(t)  */                  \
                                                                         \
                   "movl %7,%%ebx;"                                      \
                   "movl %%ebx,%%ecx;"                                   \
                   "xorl %6,%%ecx;"                                      \
                   "andl %%edx,%%ecx;"                                   \
                   "xorl %%ebx,%%ecx;"   /* ecx = F1() */                \
                                                                         \
                   "leal " #K "(%%ecx,%%eax,1),%%eax;"                   \
                   "addl %1,%%eax;"                                      \
                   /* "addl %8,%%eax;"   */                              \
                   "addl %%eax, %0;"   /*  d += temp1; */                \
                                                                         \
                   /* ----------------------------------------------------------------------- */   \
                   /* S2(a)                                    +   F0(a,b,c);                 */   \
                   /* (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))   +   ((a & b) | (c & (a | b)))  */   \
                   /* ----------------------------------------------------------------------- */   \
                                                                         \
                   "movl %2,%%ebx;"                                      \
                   "movl %3,%%ecx;"                                      \
                   "movl %%ecx,%%edx;"                                   \
                   "and  %%ebx,%%ecx;"  /* ecx = (a & b) */              \
                   "or   %%ebx,%%edx;"  /* edx = (a | b) */              \
                   "and  %4,%%edx;"                                      \
                   "or   %%edx,%%ecx;"  /* ecx = F0(); ebx = a */        \
                                                                         \
                   "rorl $2,%%ebx;"                                      \
                   "movl %%ebx,%%edx;"                                   \
                   "rorl $11,%%ebx;"                                     \
                   "xorl %%ebx,%%edx;"                                   \
                   "rorl $9,%%ebx;"                                      \
                   "xorl %%ebx,%%edx;"  /* edx = S2() */                 \
                                                                         \
                   "addl %%edx,%%ecx;"  /* ecx = Resultat */             \
                                                                         \
                   "addl %%eax,%%ecx;"  /* h = temp1 + temp2; */         \
                   "movl %%ecx, %1;"                                     \
               :"=m"(d), "=m"(h)                                                                            \
               :"m"(a), "m"(b), "m"(c), "m"(e), "m"(f), "m"(g), "i"(i), "i"(K), "m"(pW), "0"(d), "1"(h)     \
               /*   2       3       4       5       6       7       8       9       10       0       1 */   \
                                                                                                            \
               :"%eax", "%ebx", "%ecx", "%edx", "%cc", "memory")
   #elif defined(__amd64__)
      #define PR(a,b,c,d,e,f,g,h,i,K)                                         \
          __asm__  __volatile__ (                                             \
                                                                              \
                   /* ----------------------------------- */                  \
                   /* W[t] = S1(W[t -  2]) + W[t -  7] +  */                  \
                   /*        S0(W[t - 15]) + W[t - 16]    */                  \
                   /* ----------------------------------- */                  \
                                                                              \
                   "movq %10,%%rdx;"                                          \
                   "movl 4*(" #i "- 7)(%%rdx),%%ecx;"  /* ecx used for sum */ \
                   "addl 4*(" #i "-16)(%%rdx),%%ecx;"                         \
                                                                              \
                   "movl 4*(" #i "- 2)(%%rdx),%%ebx;"                         \
                   "movl %%ebx,%%eax;"                                        \
                   "shrl $10,%%eax;"                                          \
                   "rorl $17,%%ebx;"                                          \
                   "xorl %%ebx,%%eax;"                                        \
                   "rorl $2,%%ebx;"                                           \
                   "xorl %%ebx,%%eax;"  /* eax = S1() */                      \
                   "addl %%eax,%%ecx;"                                        \
                                                                              \
                   "movl 4*(" #i "-15)(%%rdx),%%ebx;"                         \
                   "movl %%ebx,%%eax;"                                        \
                   "shrl $3,%%eax;"                                           \
                   "rorl $7,%%ebx;"                                           \
                   "xorl %%ebx,%%eax;"                                        \
                   "rorl $11,%%ebx;"                                          \
                   "xorl %%ebx,%%eax;"  /* eax = S1() */                      \
                   "addl %%eax,%%ecx;"                                        \
                                                                              \
                   "movl %%ecx, 4*(" #i ")(%%rdx);" /* Write result to W[t], keep ecx for later */         \
                                                                                                           \
                   /* ------------------------------------------------------------------------------- */   \
                   /* h + S3(e)                                    +   F1(e,f,g)             + K + x  */   \
                   /* h + (ROTR(e, 6) ^ ROTR(e,11) ^ ROTR(e,25))   +   (g ^ (e & (f ^ g)))   + K + x  */   \
                   /*           $5            $5           $5           $7   $5   $6  $7       $9  $8 */   \
                   /* ------------------------------------------------------------------------------- */   \
                                                                                                           \
                   "movl %5,%%ebx;"                                      \
                   "movl %%ebx,%%edx;"                                   \
                   "rorl $6,%%ebx;"                                      \
                   "movl %%ebx,%%eax;"                                   \
                   "rorl $5,%%eax;"                                      \
                   "xorl %%ebx,%%eax;"                                   \
                   "rorl $19,%%ebx;"                                     \
                   "xorl %%ebx,%%eax;"  /* eax = S3(); edx = e */        \
                   "addl %%ecx,%%eax;"  /* Add R(t)  */                  \
                                                                         \
                   "movl %7,%%ebx;"                                      \
                   "movl %%ebx,%%ecx;"                                   \
                   "xorl %6,%%ecx;"                                      \
                   "andl %%edx,%%ecx;"                                   \
                   "xorl %%ebx,%%ecx;"   /* ecx = F1() */                \
                                                                         \
                   "leal " #K "(%%ecx,%%eax,1),%%eax;"                   \
                   "addl %1,%%eax;"                                      \
                   /* "addl %8,%%eax;"   */                              \
                   "addl %%eax, %0;"   /*  d += temp1; */                \
                                                                         \
                   /* ----------------------------------------------------------------------- */   \
                   /* S2(a)                                    +   F0(a,b,c);                 */   \
                   /* (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))   +   ((a & b) | (c & (a | b)))  */   \
                   /* ----------------------------------------------------------------------- */   \
                                                                         \
                   "movl %2,%%ebx;"                                      \
                   "movl %3,%%ecx;"                                      \
                   "movl %%ecx,%%edx;"                                   \
                   "and  %%ebx,%%ecx;"  /* ecx = (a & b) */              \
                   "or   %%ebx,%%edx;"  /* edx = (a | b) */              \
                   "and  %4,%%edx;"                                      \
                   "or   %%edx,%%ecx;"  /* ecx = F0(); ebx = a */        \
                                                                         \
                   "rorl $2,%%ebx;"                                      \
                   "movl %%ebx,%%edx;"                                   \
                   "rorl $11,%%ebx;"                                     \
                   "xorl %%ebx,%%edx;"                                   \
                   "rorl $9,%%ebx;"                                      \
                   "xorl %%ebx,%%edx;"  /* edx = S2() */                 \
                                                                         \
                   "addl %%edx,%%ecx;"  /* ecx = Resultat */             \
                                                                         \
                   "addl %%eax,%%ecx;"  /* h = temp1 + temp2; */         \
                   "movl %%ecx, %1;"                                     \
               :"=m"(d), "=m"(h)                                                                            \
               :"m"(a), "m"(b), "m"(c), "m"(e), "m"(f), "m"(g), "i"(i), "i"(K), "m"(pW), "0"(d), "1"(h)     \
               /*   2       3       4       5       6       7       8       9       10       0       1 */   \
                                                                                                            \
               :"%eax", "%ebx", "%ecx", "%edx", "%rdx", "%cc", "memory")
   #else
      #error "Processor architecture not supported by sha256 inline assembly optimisation"
   #endif
#else
// #define  SHR(x,n) ((x & 0xFFFFFFFF) >> n)  // Dieses AND mit FFFFFFFF war nur notwendig, da uint32 in sha256.h faelschlicherweise als long definiert war, was auf amd64 zu 64-Variablen fuehrte
   #define  SHR(x,n) ((x) >> n)
   #define ROTR(x,n) (SHR(x,n) | (x << (32 - n)))

   #define S0(x) (ROTR(x, 7) ^ ROTR(x,18) ^  SHR(x, 3))
   #define S1(x) (ROTR(x,17) ^ ROTR(x,19) ^  SHR(x,10))

   #define S2(x) (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))
   #define S3(x) (ROTR(x, 6) ^ ROTR(x,11) ^ ROTR(x,25))

   #define F0(x,y,z) ((x & y) | (z & (x | y)))
   #define F1(x,y,z) (z ^ (x & (y ^ z)))

   #define R(t)                                    \
   (                                               \
       W[t] = S1(W[t -  2]) + W[t -  7] +          \
              S0(W[t - 15]) + W[t - 16]            \
   )

   #define P(a,b,c,d,e,f,g,h,x,K)                  \
   {                                               \
       temp1 = h + S3(e) + F1(e,f,g) + K + x;      \
       temp2 = S2(a) + F0(a,b,c);                  \
       d += temp1; h = temp1 + temp2;              \
   }

   #define PR(a,b,c,d,e,f,g,h,i,K)                 \
      P(a,b,c,d,e,f,g,h,R(i),K)

#endif

static void SHA256Process(t_pSHA256Context pContext, uint8 data[64] )
{
    #ifndef SHA256_USE_OPTIMISED_ASSEMBLER_CODE
       uint32 temp1, temp2;
    #endif
    uint32 W[64];
    uint32 A, B, C, D, E, F, G, H;
    uint32 *pW = &W[0];

    GET_UINT32( W[0],  data,  0 );
    GET_UINT32( W[1],  data,  4 );
    GET_UINT32( W[2],  data,  8 );
    GET_UINT32( W[3],  data, 12 );
    GET_UINT32( W[4],  data, 16 );
    GET_UINT32( W[5],  data, 20 );
    GET_UINT32( W[6],  data, 24 );
    GET_UINT32( W[7],  data, 28 );
    GET_UINT32( W[8],  data, 32 );
    GET_UINT32( W[9],  data, 36 );
    GET_UINT32( W[10], data, 40 );
    GET_UINT32( W[11], data, 44 );
    GET_UINT32( W[12], data, 48 );
    GET_UINT32( W[13], data, 52 );
    GET_UINT32( W[14], data, 56 );
    GET_UINT32( W[15], data, 60 );

    A = pContext->state[0];
    B = pContext->state[1];
    C = pContext->state[2];
    D = pContext->state[3];
    E = pContext->state[4];
    F = pContext->state[5];
    G = pContext->state[6];
    H = pContext->state[7];

    P( A, B, C, D, E, F, G, H, W[ 0],  0x428A2F98 );
    P( H, A, B, C, D, E, F, G, W[ 1],  0x71374491 );
    P( G, H, A, B, C, D, E, F, W[ 2], -0x4A3F0431 );
    P( F, G, H, A, B, C, D, E, W[ 3], -0x164A245B );
    P( E, F, G, H, A, B, C, D, W[ 4],  0x3956C25B );
    P( D, E, F, G, H, A, B, C, W[ 5],  0x59F111F1 );
    P( C, D, E, F, G, H, A, B, W[ 6], -0x6DC07D5C );
    P( B, C, D, E, F, G, H, A, W[ 7], -0x54E3A12B );
    P( A, B, C, D, E, F, G, H, W[ 8], -0x27F85568 );
    P( H, A, B, C, D, E, F, G, W[ 9],  0x12835B01 );
    P( G, H, A, B, C, D, E, F, W[10],  0x243185BE );
    P( F, G, H, A, B, C, D, E, W[11],  0x550C7DC3 );
    P( E, F, G, H, A, B, C, D, W[12],  0x72BE5D74 );
    P( D, E, F, G, H, A, B, C, W[13], -0x7F214E02 );
    P( C, D, E, F, G, H, A, B, W[14], -0x6423F959 );
    P( B, C, D, E, F, G, H, A, W[15], -0x3E640E8C );
    PR(A, B, C, D, E, F, G, H,   16 , -0x1B64963F );
    PR(H, A, B, C, D, E, F, G,   17 , -0x1041B87A );
    PR(G, H, A, B, C, D, E, F,   18 ,  0x0FC19DC6 );
    PR(F, G, H, A, B, C, D, E,   19 ,  0x240CA1CC );
    PR(E, F, G, H, A, B, C, D,   20 ,  0x2DE92C6F );
    PR(D, E, F, G, H, A, B, C,   21 ,  0x4A7484AA );
    PR(C, D, E, F, G, H, A, B,   22 ,  0x5CB0A9DC );
    PR(B, C, D, E, F, G, H, A,   23 ,  0x76F988DA );
    PR(A, B, C, D, E, F, G, H,   24 , -0x67C1AEAE );
    PR(H, A, B, C, D, E, F, G,   25 , -0x57CE3993 );
    PR(G, H, A, B, C, D, E, F,   26 , -0x4FFCD838 );
    PR(F, G, H, A, B, C, D, E,   27 , -0x40A68039 );
    PR(E, F, G, H, A, B, C, D,   28 , -0x391FF40D );
    PR(D, E, F, G, H, A, B, C,   29 , -0x2A586EB9 );
    PR(C, D, E, F, G, H, A, B,   30 ,  0x06CA6351 );
    PR(B, C, D, E, F, G, H, A,   31 ,  0x14292967 );
    PR(A, B, C, D, E, F, G, H,   32 ,  0x27B70A85 );
    PR(H, A, B, C, D, E, F, G,   33 ,  0x2E1B2138 );
    PR(G, H, A, B, C, D, E, F,   34 ,  0x4D2C6DFC );
    PR(F, G, H, A, B, C, D, E,   35 ,  0x53380D13 );
    PR(E, F, G, H, A, B, C, D,   36 ,  0x650A7354 );
    PR(D, E, F, G, H, A, B, C,   37 ,  0x766A0ABB );
    PR(C, D, E, F, G, H, A, B,   38 , -0x7E3D36D2 );
    PR(B, C, D, E, F, G, H, A,   39 , -0x6D8DD37B );
    PR(A, B, C, D, E, F, G, H,   40 , -0x5D40175F );
    PR(H, A, B, C, D, E, F, G,   41 , -0x57E599B5 );
    PR(G, H, A, B, C, D, E, F,   42 , -0x3DB47490 );
    PR(F, G, H, A, B, C, D, E,   43 , -0x3893AE5D );
    PR(E, F, G, H, A, B, C, D,   44 , -0x2E6D17E7 );
    PR(D, E, F, G, H, A, B, C,   45 , -0x2966F9DC );
    PR(C, D, E, F, G, H, A, B,   46 , -0x0BF1CA7B );
    PR(B, C, D, E, F, G, H, A,   47 ,  0x106AA070 );
    PR(A, B, C, D, E, F, G, H,   48 ,  0x19A4C116 );
    PR(H, A, B, C, D, E, F, G,   49 ,  0x1E376C08 );
    PR(G, H, A, B, C, D, E, F,   50 ,  0x2748774C );
    PR(F, G, H, A, B, C, D, E,   51 ,  0x34B0BCB5 );
    PR(E, F, G, H, A, B, C, D,   52 ,  0x391C0CB3 );
    PR(D, E, F, G, H, A, B, C,   53 ,  0x4ED8AA4A );
    PR(C, D, E, F, G, H, A, B,   54 ,  0x5B9CCA4F );
    PR(B, C, D, E, F, G, H, A,   55 ,  0x682E6FF3 );
    PR(A, B, C, D, E, F, G, H,   56 ,  0x748F82EE );
    PR(H, A, B, C, D, E, F, G,   57 ,  0x78A5636F );
    PR(G, H, A, B, C, D, E, F,   58 , -0x7B3787EC );
    PR(F, G, H, A, B, C, D, E,   59 , -0x7338FDF8 );
    PR(E, F, G, H, A, B, C, D,   60 , -0x6F410006 );
    PR(D, E, F, G, H, A, B, C,   61 , -0x5BAF9315 );
    PR(C, D, E, F, G, H, A, B,   62 , -0x41065C09 );
    PR(B, C, D, E, F, G, H, A,   63 , -0x398E870E );

    pContext->state[0] += A;
    pContext->state[1] += B;
    pContext->state[2] += C;
    pContext->state[3] += D;
    pContext->state[4] += E;
    pContext->state[5] += F;
    pContext->state[6] += G;
    pContext->state[7] += H;
}

void SHA256Append (t_pSHA256Context pContext, uint8 *input, uint32 length )
{
    uint32 left, fill;

    if( ! length ) return;

    left = pContext->total[0] & 0x3F;
    fill = 64 - left;

    pContext->total[0] += length;
    pContext->total[0] &= 0xFFFFFFFF;

    if( pContext->total[0] < length )
        pContext->total[1]++;

    if( left && length >= fill )
    {
        memcpy( (void *) (pContext->buffer + left),
                (void *) input, fill );
        SHA256Process (pContext, pContext->buffer);
        length -= fill;
        input  += fill;
        left = 0;
    }

    while( length >= 64 )
    {
        SHA256Process (pContext, input );
        length -= 64;
        input  += 64;
    }

    if( length )
    {
        memcpy( (void *) (pContext->buffer + left),
                (void *) input, length );
    }
}

static uint8 SHA256Padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void SHA256Finish (t_pSHA256Context pContext, uint8 digest[32])
{
    uint32 last, padn;
    uint32 high, low;
    uint8 msglen[8];

    high = ( pContext->total[0] >> 29 )
         | ( pContext->total[1] <<  3 );
    low  = ( pContext->total[0] <<  3 );

    PUT_UINT32( high, msglen, 0 );
    PUT_UINT32( low,  msglen, 4 );

    last = pContext->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    SHA256Append (pContext, SHA256Padding, padn);
    SHA256Append (pContext, msglen, 8);

    PUT_UINT32(pContext->state[0], digest,  0);
    PUT_UINT32(pContext->state[1], digest,  4);
    PUT_UINT32(pContext->state[2], digest,  8);
    PUT_UINT32(pContext->state[3], digest, 12);
    PUT_UINT32(pContext->state[4], digest, 16);
    PUT_UINT32(pContext->state[5], digest, 20);
    PUT_UINT32(pContext->state[6], digest, 24);
    PUT_UINT32(pContext->state[7], digest, 28);
}

#else  // SHA256_OLD

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef WORDS_BIGENDIAN
   #define SWAP(n) (n)
#else
   #define SWAP(n) (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#endif

#define BLOCKSIZE 32768
#if BLOCKSIZE % 64 != 0
  #error "invalid BLOCKSIZE"
#endif

static const unsigned char SHA256PadBuf[64] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void SHA256Init (t_pSHA256Context pContext)
{
   pContext->state[0] = 0x6a09e667UL;
   pContext->state[1] = 0xbb67ae85UL;
   pContext->state[2] = 0x3c6ef372UL;
   pContext->state[3] = 0xa54ff53aUL;
   pContext->state[4] = 0x510e527fUL;
   pContext->state[5] = 0x9b05688cUL;
   pContext->state[6] = 0x1f83d9abUL;
   pContext->state[7] = 0x5be0cd19UL;

   pContext->total[0] = pContext->total[1] = 0;
   pContext->buflen   = 0;
}

static void set_uint32 (char *cp, uint32 v)
{
   memcpy (cp, &v, sizeof v);
}

// SHA256 round constants
#define K(I) sha256_round_constants[I]
static const uint32 sha256_round_constants[64] =
{
   0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
   0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
   0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
   0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
   0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
   0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
   0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
   0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
   0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
   0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
   0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
   0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
   0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
   0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
   0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
   0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

// Round functions
#define F2(A,B,C) ( ( A & B ) | ( C & ( A | B ) ) )
#define F1(E,F,G) ( G ^ ( E & ( F ^ G ) ) )

static void SHA256ProcessBlock (t_pSHA256Context pContext, const void *buffer, size_t len) __attribute__((optimize("-O3")));
static void SHA256ProcessBlock (t_pSHA256Context pContext, const void *buffer, size_t len)
{
   size_t        nwords = len / sizeof (uint32);
   const uint32 *words  = (uint32 *)buffer;
   const uint32 *endp   = words + nwords;
   uint32 a = pContext->state[0];
   uint32 b = pContext->state[1];
   uint32 c = pContext->state[2];
   uint32 d = pContext->state[3];
   uint32 e = pContext->state[4];
   uint32 f = pContext->state[5];
   uint32 g = pContext->state[6];
   uint32 h = pContext->state[7];
   uint32 x[16];

   pContext->total[0] += len;
   if (pContext->total[0] < len)
      ++pContext->total[1];

   #define rol(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
   #define S0(x)     (rol(x,25) ^ rol(x,14) ^ (x>> 3))
   #define S1(x)     (rol(x,15) ^ rol(x,13) ^ (x>>10))
   #define SS0(x)    (rol(x,30) ^ rol(x,19) ^ rol(x,10))
   #define SS1(x)    (rol(x,26) ^ rol(x,21) ^ rol(x, 7))

   #define M(I) ( tm =   S1(x[(I- 2)&0x0f]) + x[(I-7)&0x0f] \
                       + S0(x[(I-15)&0x0f]) + x[ I   &0x0f] \
                  , x[I&0x0f] = tm )

   #define R(A,B,C,D,E,F,G,H,K,M)  \
      do                           \
      {                            \
         t0  = SS0(A) + F2(A,B,C); \
         t1  = H + SS1(E)          \
             + F1(E,F,G)           \
             + K                   \
             + M;                  \
          D += t1;                 \
          H  = t0 + t1;            \
      } while(0)

   while (words < endp)
   {
      uint32 tm;
      uint32 t0, t1;
      int t;

      for (t = 0; t < 16; t++)
      {
         x[t] = SWAP (*words);
         words++;
      }

      R( a, b, c, d, e, f, g, h, K( 0), x[ 0] );
      R( h, a, b, c, d, e, f, g, K( 1), x[ 1] );
      R( g, h, a, b, c, d, e, f, K( 2), x[ 2] );
      R( f, g, h, a, b, c, d, e, K( 3), x[ 3] );
      R( e, f, g, h, a, b, c, d, K( 4), x[ 4] );
      R( d, e, f, g, h, a, b, c, K( 5), x[ 5] );
      R( c, d, e, f, g, h, a, b, K( 6), x[ 6] );
      R( b, c, d, e, f, g, h, a, K( 7), x[ 7] );
      R( a, b, c, d, e, f, g, h, K( 8), x[ 8] );
      R( h, a, b, c, d, e, f, g, K( 9), x[ 9] );
      R( g, h, a, b, c, d, e, f, K(10), x[10] );
      R( f, g, h, a, b, c, d, e, K(11), x[11] );
      R( e, f, g, h, a, b, c, d, K(12), x[12] );
      R( d, e, f, g, h, a, b, c, K(13), x[13] );
      R( c, d, e, f, g, h, a, b, K(14), x[14] );
      R( b, c, d, e, f, g, h, a, K(15), x[15] );
      R( a, b, c, d, e, f, g, h, K(16), M(16) );
      R( h, a, b, c, d, e, f, g, K(17), M(17) );
      R( g, h, a, b, c, d, e, f, K(18), M(18) );
      R( f, g, h, a, b, c, d, e, K(19), M(19) );
      R( e, f, g, h, a, b, c, d, K(20), M(20) );
      R( d, e, f, g, h, a, b, c, K(21), M(21) );
      R( c, d, e, f, g, h, a, b, K(22), M(22) );
      R( b, c, d, e, f, g, h, a, K(23), M(23) );
      R( a, b, c, d, e, f, g, h, K(24), M(24) );
      R( h, a, b, c, d, e, f, g, K(25), M(25) );
      R( g, h, a, b, c, d, e, f, K(26), M(26) );
      R( f, g, h, a, b, c, d, e, K(27), M(27) );
      R( e, f, g, h, a, b, c, d, K(28), M(28) );
      R( d, e, f, g, h, a, b, c, K(29), M(29) );
      R( c, d, e, f, g, h, a, b, K(30), M(30) );
      R( b, c, d, e, f, g, h, a, K(31), M(31) );
      R( a, b, c, d, e, f, g, h, K(32), M(32) );
      R( h, a, b, c, d, e, f, g, K(33), M(33) );
      R( g, h, a, b, c, d, e, f, K(34), M(34) );
      R( f, g, h, a, b, c, d, e, K(35), M(35) );
      R( e, f, g, h, a, b, c, d, K(36), M(36) );
      R( d, e, f, g, h, a, b, c, K(37), M(37) );
      R( c, d, e, f, g, h, a, b, K(38), M(38) );
      R( b, c, d, e, f, g, h, a, K(39), M(39) );
      R( a, b, c, d, e, f, g, h, K(40), M(40) );
      R( h, a, b, c, d, e, f, g, K(41), M(41) );
      R( g, h, a, b, c, d, e, f, K(42), M(42) );
      R( f, g, h, a, b, c, d, e, K(43), M(43) );
      R( e, f, g, h, a, b, c, d, K(44), M(44) );
      R( d, e, f, g, h, a, b, c, K(45), M(45) );
      R( c, d, e, f, g, h, a, b, K(46), M(46) );
      R( b, c, d, e, f, g, h, a, K(47), M(47) );
      R( a, b, c, d, e, f, g, h, K(48), M(48) );
      R( h, a, b, c, d, e, f, g, K(49), M(49) );
      R( g, h, a, b, c, d, e, f, K(50), M(50) );
      R( f, g, h, a, b, c, d, e, K(51), M(51) );
      R( e, f, g, h, a, b, c, d, K(52), M(52) );
      R( d, e, f, g, h, a, b, c, K(53), M(53) );
      R( c, d, e, f, g, h, a, b, K(54), M(54) );
      R( b, c, d, e, f, g, h, a, K(55), M(55) );
      R( a, b, c, d, e, f, g, h, K(56), M(56) );
      R( h, a, b, c, d, e, f, g, K(57), M(57) );
      R( g, h, a, b, c, d, e, f, K(58), M(58) );
      R( f, g, h, a, b, c, d, e, K(59), M(59) );
      R( e, f, g, h, a, b, c, d, K(60), M(60) );
      R( d, e, f, g, h, a, b, c, K(61), M(61) );
      R( c, d, e, f, g, h, a, b, K(62), M(62) );
      R( b, c, d, e, f, g, h, a, K(63), M(63) );

      a = pContext->state[0] += a;
      b = pContext->state[1] += b;
      c = pContext->state[2] += c;
      d = pContext->state[3] += d;
      e = pContext->state[4] += e;
      f = pContext->state[5] += f;
      g = pContext->state[6] += g;
      h = pContext->state[7] += h;
   }
}

void SHA256Finish (t_pSHA256Context pContext, void *pDigest)
{
   // Take yet unprocessed bytes into account
   size_t bytes = pContext->buflen;
   size_t size = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

   // Count remaining bytes
   pContext->total[0] += bytes;
   if (pContext->total[0] < bytes)
      ++pContext->total[1];

   set_uint32 ((char *) &pContext->buffer[size - 2], SWAP ((pContext->total[1] << 3) | (pContext->total[0] >> 29)));
   set_uint32 ((char *) &pContext->buffer[size - 1], SWAP (pContext->total[0] << 3));

   memcpy (&((char *) pContext->buffer)[bytes], SHA256PadBuf, (size - 2) * 4 - bytes);

   // Process last bytes
   SHA256ProcessBlock (pContext, pContext->buffer, size * 4);

   // Get digest
   int   i;
   char *r = (char*)pDigest;

   for (i = 0; i < 8; i++)
      set_uint32 (r + i * sizeof pContext->state[0], SWAP (pContext->state[i]));
}


void SHA256Append (t_pSHA256Context pContext, const void *buffer, size_t len)
{
   // When we already have some bits in our internal buffer concatenate both inputs first
   if (pContext->buflen != 0)
   {
      size_t left_over = pContext->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&((char *) pContext->buffer)[left_over], buffer, add);
      pContext->buflen += add;

      if (pContext->buflen > 64)
      {
         SHA256ProcessBlock (pContext, pContext->buffer, pContext->buflen & ~63);

         pContext->buflen &= 63;
         memcpy (pContext->buffer,
                 &((char *) pContext->buffer)[(left_over + add) & ~63],
                 pContext->buflen);
      }
      buffer = (const char *) buffer + add;
      len -= add;
   }

   // Process available complete blocks
   if (len >= 64)
   {
      #if !_STRING_ARCH_unaligned
         #define UNALIGNED_P(p) ((uintptr_t) (p) % alignof (uint32_t) != 0)
         if (UNALIGNED_P (buffer))
         {
            while (len > 64)
            {
               SHA256ProcessBlock (pContext, memcpy (pContext->buffer, buffer, 64), 64);
               buffer = (const char *) buffer + 64;
               len -= 64;
            }
         }
         else
      #endif
         {
            SHA256ProcessBlock (pContext, buffer, len & ~63);
            buffer = (const char *) buffer + (len & ~63);
            len &= 63;
         }
   }

   // Move remaining bytes in internal buffer
   if (len > 0)
   {
      size_t left_over = pContext->buflen;

      memcpy (&((char *) pContext->buffer)[left_over], buffer, len);
      left_over += len;
      if (left_over >= 64)
      {
         SHA256ProcessBlock (pContext, pContext->buffer, 64);
         left_over -= 64;
         memcpy (pContext->buffer, &pContext->buffer[16], left_over);
      }
      pContext->buflen = left_over;
   }
}


#endif // SHA256_OLD

#ifdef TEST

#include <stdlib.h>
#include <stdio.h>

/*
 * those are the standard FIPS-180-2 test vectors
 */

static const char *msg[] =
{
    "abc",
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
    nullptr
};

static const char *val[] =
{
    "ba7816bf8f01cfea414140de5dae2223" \
    "b00361a396177a9cb410ff61f20015ad",
    "248d6a61d20638b8e5c026930c3e6039" \
    "a33ce45964ff2167f6ecedd419db06c1",
    "cdc76e5c9914fb9281a1c7e284d73e67" \
    "f1809a48a497200e046d39ccc7112cd0"
};

int main( int argc, char *argv[] )
{
   FILE            *f;
   int             i, j;
   char            output[65];
   t_SHA256Context Context;
   unsigned char   buf[65536];
   unsigned char   sha256sum[32];

   if( argc < 2 )
   {
      printf( "\n SHA-256 Validation Tests:\n\n" );

      for( i = 0; i < 3; i++ )
      {
         printf( " Test %d ", i + 1 );
         SHA256Init (&Context);
         if( i < 2 )
         {
            SHA256Append (&Context, (uint8 *) msg[i], strlen( msg[i] ) );
         }
         else
         {
            memset( buf, 'a', 1000 );
            for (j = 0; j < 1000; j++)
               SHA256Append (&Context, (uint8 *) buf, 1000 );
         }

         SHA256Finish (&Context, sha256sum);
         for (j = 0; j < 32; j++)
            sprintf( output + j * 2, "%02x", sha256sum[j] );

         if (memcmp( output, val[i], 64 ))
         {
            printf( "failed!\n" );
            return( 1 );
         }
         printf( "passed.\n" );
      }
      printf( "\n" );
   }
   else
   {
      if (!(f = fopen( argv[1], "rb")))
      {
         perror( "fopen" );
         return( 1 );
     }

     SHA256Init (&Context);
     while ((i = fread(buf, 1, sizeof(buf), f)) > 0)
        SHA256Append (&Context, buf, i );
     SHA256Finish (&Context, sha256sum);

     for (j = 0; j<32; j++)
        printf( "%02x", sha256sum[j] );

     printf( "  %s\n", argv[1] );
   }
   return (0);
}

#endif
