// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         MD5 calculation
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

// This module is based on the source code of standard Linux' md5sum command
// (see Debian package coreutils, for example). The core functions have been
// mainly copy/pasted; they have been written by David Madore and Scott G.
// Miller and are copyrighted by the Free Software Foundation, Inc.


#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "md5.h"

// ----------------------
//  Macros and constants
// ----------------------

static const unsigned char MD5PadBuf[64] = { 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#ifdef WORDS_BIGENDIAN
   #define SWAP(n)                                                        \
      (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
   #define SWAP(n) (n)
#endif

// -----------
//  Functions
// -----------

void MD5Init (t_MD5Context *pContext)
{
   pContext->A = 0x67452301;
   pContext->B = 0xefcdab89;
   pContext->C = 0x98badcfe;
   pContext->D = 0x10325476;

   pContext->total[0] = pContext->total[1] = 0;
   pContext->buflen = 0;
}

static void set_uint32 (char *cp, uint32_t v)
{
   memcpy (cp, &v, sizeof v);
}

#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

static void MD5ProcessBlock (t_pMD5Context pContext, const void *buffer, size_t len) __attribute__((optimize("-O3")));
static void MD5ProcessBlock (t_pMD5Context pContext, const void *buffer, size_t len)
{
   uint32_t        correct_words[16];
   size_t          nwords = len / sizeof (uint32_t);
   const uint32_t *words  = (uint32_t *)buffer;
   const uint32_t *endp   = words + nwords;
   uint32_t        A = pContext->A;
   uint32_t        B = pContext->B;
   uint32_t        C = pContext->C;
   uint32_t        D = pContext->D;

   // Increment byte count
   pContext->total[0] += len;
   if (pContext->total[0] < len)
      ++pContext->total[1];

   while (words < endp)
   {
      uint32_t *cwp = correct_words;
      uint32_t A_save = A;
      uint32_t B_save = B;
      uint32_t C_save = C;
      uint32_t D_save = D;

      // We perhaps have to change the byte order before the computation.  To reduce the
      // work for the next steps we store the swapped words in the array correct_words.

      #define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))
      #define OP(a, b, c, d, s, T)                            \
         do                                                   \
         {                                                    \
            a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T; \
            ++words;                                          \
            CYCLIC (a, s);                                    \
            a += b;                                           \
         }                                                    \
         while (0);

      // Round 1
      OP (A, B, C, D,  7, 0xd76aa478)
      OP (D, A, B, C, 12, 0xe8c7b756)
      OP (C, D, A, B, 17, 0x242070db)
      OP (B, C, D, A, 22, 0xc1bdceee)
      OP (A, B, C, D,  7, 0xf57c0faf)
      OP (D, A, B, C, 12, 0x4787c62a)
      OP (C, D, A, B, 17, 0xa8304613)
      OP (B, C, D, A, 22, 0xfd469501)
      OP (A, B, C, D,  7, 0x698098d8)
      OP (D, A, B, C, 12, 0x8b44f7af)
      OP (C, D, A, B, 17, 0xffff5bb1)
      OP (B, C, D, A, 22, 0x895cd7be)
      OP (A, B, C, D,  7, 0x6b901122)
      OP (D, A, B, C, 12, 0xfd987193)
      OP (C, D, A, B, 17, 0xa679438e)
      OP (B, C, D, A, 22, 0x49b40821)

      #undef OP
      #define OP(f, a, b, c, d, k, s, T)             \
         do                                          \
         {                                           \
            a += f (b, c, d) + correct_words[k] + T; \
            CYCLIC (a, s);                           \
            a += b;                                  \
         }                                           \
         while (0);

      // Round 2
      OP (FG, A, B, C, D,  1,  5, 0xf61e2562)
      OP (FG, D, A, B, C,  6,  9, 0xc040b340)
      OP (FG, C, D, A, B, 11, 14, 0x265e5a51)
      OP (FG, B, C, D, A,  0, 20, 0xe9b6c7aa)
      OP (FG, A, B, C, D,  5,  5, 0xd62f105d)
      OP (FG, D, A, B, C, 10,  9, 0x02441453)
      OP (FG, C, D, A, B, 15, 14, 0xd8a1e681)
      OP (FG, B, C, D, A,  4, 20, 0xe7d3fbc8)
      OP (FG, A, B, C, D,  9,  5, 0x21e1cde6)
      OP (FG, D, A, B, C, 14,  9, 0xc33707d6)
      OP (FG, C, D, A, B,  3, 14, 0xf4d50d87)
      OP (FG, B, C, D, A,  8, 20, 0x455a14ed)
      OP (FG, A, B, C, D, 13,  5, 0xa9e3e905)
      OP (FG, D, A, B, C,  2,  9, 0xfcefa3f8)
      OP (FG, C, D, A, B,  7, 14, 0x676f02d9)
      OP (FG, B, C, D, A, 12, 20, 0x8d2a4c8a)

      // Round 3
      OP (FH, A, B, C, D,  5,  4, 0xfffa3942)
      OP (FH, D, A, B, C,  8, 11, 0x8771f681)
      OP (FH, C, D, A, B, 11, 16, 0x6d9d6122)
      OP (FH, B, C, D, A, 14, 23, 0xfde5380c)
      OP (FH, A, B, C, D,  1,  4, 0xa4beea44)
      OP (FH, D, A, B, C,  4, 11, 0x4bdecfa9)
      OP (FH, C, D, A, B,  7, 16, 0xf6bb4b60)
      OP (FH, B, C, D, A, 10, 23, 0xbebfbc70)
      OP (FH, A, B, C, D, 13,  4, 0x289b7ec6)
      OP (FH, D, A, B, C,  0, 11, 0xeaa127fa)
      OP (FH, C, D, A, B,  3, 16, 0xd4ef3085)
      OP (FH, B, C, D, A,  6, 23, 0x04881d05)
      OP (FH, A, B, C, D,  9,  4, 0xd9d4d039)
      OP (FH, D, A, B, C, 12, 11, 0xe6db99e5)
      OP (FH, C, D, A, B, 15, 16, 0x1fa27cf8)
      OP (FH, B, C, D, A,  2, 23, 0xc4ac5665)

      // Round 4
      OP (FI, A, B, C, D,  0,  6, 0xf4292244)
      OP (FI, D, A, B, C,  7, 10, 0x432aff97)
      OP (FI, C, D, A, B, 14, 15, 0xab9423a7)
      OP (FI, B, C, D, A,  5, 21, 0xfc93a039)
      OP (FI, A, B, C, D, 12,  6, 0x655b59c3)
      OP (FI, D, A, B, C,  3, 10, 0x8f0ccc92)
      OP (FI, C, D, A, B, 10, 15, 0xffeff47d)
      OP (FI, B, C, D, A,  1, 21, 0x85845dd1)
      OP (FI, A, B, C, D,  8,  6, 0x6fa87e4f)
      OP (FI, D, A, B, C, 15, 10, 0xfe2ce6e0)
      OP (FI, C, D, A, B,  6, 15, 0xa3014314)
      OP (FI, B, C, D, A, 13, 21, 0x4e0811a1)
      OP (FI, A, B, C, D,  4,  6, 0xf7537e82)
      OP (FI, D, A, B, C, 11, 10, 0xbd3af235)
      OP (FI, C, D, A, B,  2, 15, 0x2ad7d2bb)
      OP (FI, B, C, D, A,  9, 21, 0xeb86d391)

      A += A_save;
      B += B_save;
      C += C_save;
      D += D_save;
   }

   pContext->A = A;
   pContext->B = B;
   pContext->C = C;
   pContext->D = D;
}

// The following code had been written for test purposes by myself (Guy). It contains the core MD5 calculation
// in assembler. It has been tested and runs correctly, but it turned out to run slower than md5sum or other
// g++ optimised code.
// 1:0 für g++  :-)

#ifdef MD5_USE_ASSEMBLER_CODE
   static void MD5Process (t_pMD5Context pContext, const t_uchar *pData) // pData points to block of 64 bytes
   {
      // F(r2,r3,r4) = (((r3 ^ r4) & r2) ^ r4)
      // r1 = ROTATE_LEFT(r1 + F(r2,r3,r4) + *X++ + Ti, s) + r2
      #define OP_ROUND_1(r1,r2,r3,r4,Ofs,Shift,T) \
         "movl " #r4 ",%%esi;" \
         "xorl " #r3 ",%%esi;" \
         "andl " #r2 ",%%esi;" \
         "xorl " #r4 ",%%esi;" \
         "leal " #T "(" #r1 ",%%esi,1)," #r1 ";"                   \
         "addl " #Ofs "(%%rdi)," #r1 ";" \
         "roll $" #Shift "," #r1 ";" \
         "addl " #r2 "," #r1 ";" \

      // G(r2,r3,r4) = (((r2 ^ r3) & r4) ^ r3)
      // r1 = ROTATE_LEFT(r1 + G(r2,r3,r4) + X[k] + Ti, s) + r2
      #define OP_ROUND_2(r1,r2,r3,r4,Ofs,Shift,T) \
         "movl " #r3 ",%%esi;" \
         "xorl " #r2 ",%%esi;" \
         "andl " #r4 ",%%esi;" \
         "xorl " #r3 ",%%esi;" \
         "leal " #T "(" #r1 ",%%esi,1)," #r1 ";"                   \
         "addl " #Ofs "(%%rdi)," #r1 ";" \
         "roll $" #Shift "," #r1 ";" \
         "addl " #r2 "," #r1 ";" \

      //  H(r2,r3,r4) = (r2 ^ r3 ^ r4)
      //  r1 = ROTATE_LEFT(r1 + H(r2,r3,r4) + X[k] + Ti, s) + r2
      #define OP_ROUND_3(r1,r2,r3,r4,Ofs,Shift,T) \
         "movl " #r2 ",%%esi;" \
         "xorl " #r3 ",%%esi;" \
         "xorl " #r4 ",%%esi;" \
         "leal " #T "(" #r1 ",%%esi,1)," #r1 ";"                   \
         "addl " #Ofs "(%%rdi)," #r1 ";" \
         "roll $" #Shift "," #r1 ";" \
         "addl " #r2 "," #r1 ";" \

      //  I(r2,r3,r4) = ((r2 | ~r4) ^ r3)
      //  r1 = ROTATE_LEFT(r1 + I(r2,r3,r4) + X[k] + Ti, s) + r2
      #define OP_ROUND_4(r1,r2,r3,r4,Ofs,Shift,T) \
         "movl " #r4 ",%%esi;" \
         "not %%esi;" \
         "orl  " #r2 ",%%esi;" \
         "xorl " #r3 ",%%esi;" \
         "leal " #T "(" #r1 ",%%esi,1)," #r1 ";"                   \
         "addl " #Ofs "(%%rdi)," #r1 ";" \
         "roll $" #Shift "," #r1 ";" \
         "addl " #r2 "," #r1 ";" \

      __asm__
      (                                                                          \
         OP_ROUND_1(%4,%5,%6,%7,  0,  7, -0x28955b88)
         OP_ROUND_1(%7,%4,%5,%6,  4, 12, -0x173848aa)
         OP_ROUND_1(%6,%7,%4,%5,  8, 17,  0x242070db)
         OP_ROUND_1(%5,%6,%7,%4, 12, 22, -0x3e423112)
         OP_ROUND_1(%4,%5,%6,%7, 16,  7, -0x0a83f051)
         OP_ROUND_1(%7,%4,%5,%6, 20, 12,  0x4787c62a)
         OP_ROUND_1(%6,%7,%4,%5, 24, 17, -0x57cfb9ed)
         OP_ROUND_1(%5,%6,%7,%4, 28, 22, -0x02b96aff)
         OP_ROUND_1(%4,%5,%6,%7, 32,  7,  0x698098d8)
         OP_ROUND_1(%7,%4,%5,%6, 36, 12, -0x74bb0851)
         OP_ROUND_1(%6,%7,%4,%5, 40, 17, -0x0000a44f)
         OP_ROUND_1(%5,%6,%7,%4, 44, 22, -0x76a32842)
         OP_ROUND_1(%4,%5,%6,%7, 48,  7,  0x6b901122)
         OP_ROUND_1(%7,%4,%5,%6, 52, 12, -0x02678e6d)
         OP_ROUND_1(%6,%7,%4,%5, 56, 17, -0x5986bc72)
         OP_ROUND_1(%5,%6,%7,%4, 60, 22,  0x49b40821)

         OP_ROUND_2(%4,%5,%6,%7,  4,  5, -0x09e1da9e)
         OP_ROUND_2(%7,%4,%5,%6, 24,  9, -0x3fbf4cc0)
         OP_ROUND_2(%6,%7,%4,%5, 44, 14,  0x265e5a51)
         OP_ROUND_2(%5,%6,%7,%4,  0, 20, -0x16493856)
         OP_ROUND_2(%4,%5,%6,%7, 20,  5, -0x29d0efa3)
         OP_ROUND_2(%7,%4,%5,%6, 40,  9,  0x02441453)
         OP_ROUND_2(%6,%7,%4,%5, 60, 14, -0x275e197f)
         OP_ROUND_2(%5,%6,%7,%4, 16, 20, -0x182c0438)
         OP_ROUND_2(%4,%5,%6,%7, 36,  5,  0x21e1cde6)
         OP_ROUND_2(%7,%4,%5,%6, 56,  9, -0x3cc8f82a)
         OP_ROUND_2(%6,%7,%4,%5, 12, 14, -0x0b2af279)
         OP_ROUND_2(%5,%6,%7,%4, 32, 20,  0x455a14ed)
         OP_ROUND_2(%4,%5,%6,%7, 52,  5, -0x561c16fb)
         OP_ROUND_2(%7,%4,%5,%6,  8,  9, -0x03105c08)
         OP_ROUND_2(%6,%7,%4,%5, 28, 14,  0x676f02d9)
         OP_ROUND_2(%5,%6,%7,%4, 48, 20, -0x72d5b376)

         OP_ROUND_3(%4,%5,%6,%7, 20,  4, -0x0005c6be)
         OP_ROUND_3(%7,%4,%5,%6, 32, 11, -0x788e097f)
         OP_ROUND_3(%6,%7,%4,%5, 44, 16,  0x6d9d6122)
         OP_ROUND_3(%5,%6,%7,%4, 56, 23, -0x021ac7f4)
         OP_ROUND_3(%4,%5,%6,%7,  4,  4, -0x5b4115bc)
         OP_ROUND_3(%7,%4,%5,%6, 16, 11,  0x4bdecfa9)
         OP_ROUND_3(%6,%7,%4,%5, 28, 16, -0x0944b4a0)
         OP_ROUND_3(%5,%6,%7,%4, 40, 23, -0x41404390)
         OP_ROUND_3(%4,%5,%6,%7, 52,  4,  0x289b7ec6)
         OP_ROUND_3(%7,%4,%5,%6,  0, 11, -0x155ed806)
         OP_ROUND_3(%6,%7,%4,%5, 12, 16, -0x2b10cf7b)
         OP_ROUND_3(%5,%6,%7,%4, 24, 23,  0x04881d05)
         OP_ROUND_3(%4,%5,%6,%7, 36,  4, -0x262b2fc7)
         OP_ROUND_3(%7,%4,%5,%6, 48, 11, -0x1924661b)
         OP_ROUND_3(%6,%7,%4,%5, 60, 16,  0x1fa27cf8)
         OP_ROUND_3(%5,%6,%7,%4,  8, 23, -0x3b53a99b)

         OP_ROUND_4(%4,%5,%6,%7,  0,  6, -0x0bd6ddbc)
         OP_ROUND_4(%7,%4,%5,%6, 28, 10,  0x432aff97)
         OP_ROUND_4(%6,%7,%4,%5, 56, 15, -0x546bdc59)
         OP_ROUND_4(%5,%6,%7,%4, 20, 21, -0x036c5fc7)
         OP_ROUND_4(%4,%5,%6,%7, 48,  6,  0x655b59c3)
         OP_ROUND_4(%7,%4,%5,%6, 12, 10, -0x70f3336e)
         OP_ROUND_4(%6,%7,%4,%5, 40, 15, -0x00100b83)
         OP_ROUND_4(%5,%6,%7,%4,  4, 21, -0x7a7ba22f)
         OP_ROUND_4(%4,%5,%6,%7, 32,  6,  0x6fa87e4f)
         OP_ROUND_4(%7,%4,%5,%6, 60, 10, -0x01d31920)
         OP_ROUND_4(%6,%7,%4,%5, 24, 15, -0x5cfebcec)
         OP_ROUND_4(%5,%6,%7,%4, 52, 21,  0x4e0811a1)
         OP_ROUND_4(%4,%5,%6,%7, 16,  6, -0x08ac817e)
         OP_ROUND_4(%7,%4,%5,%6, 44, 10, -0x42c50dcb)
         OP_ROUND_4(%6,%7,%4,%5,  8, 15,  0x2ad7d2bb)
         OP_ROUND_4(%5,%6,%7,%4, 36, 21, -0x14792c6f)

         "addl %4,%0;"
         "addl %5,%1;"
         "addl %6,%2;"
         "addl %7,%3;"
         :"=m"(pContext->a), "=m"(pContext->b), "=m"(pContext->c), "=m"(pContext->d) \
         : "r"(pContext->a),  "r"(pContext->b),  "r"(pContext->c),  "r"(pContext->d), "D"(pData)       \
         : "%esi", "%cc"
      );
}
#endif

void MD5Append (t_pMD5Context pContext, const void *pBuffer, size_t Len)
{
   // When we already have some bits in our internal buffer concatenate both inputs first.
   if (pContext->buflen != 0)
   {
      size_t left_over = pContext->buflen;
      size_t add       = 128 - left_over > Len ? Len : 128 - left_over;

      memcpy (&((char *) pContext->buffer)[left_over], pBuffer, add);
      pContext->buflen += add;

      if (pContext->buflen > 64)
      {
         MD5ProcessBlock (pContext, pContext->buffer, pContext->buflen & ~63);

         pContext->buflen &= 63;
         memcpy (pContext->buffer,
                 &((char *) pContext->buffer)[(left_over + add) & ~63],
                 pContext->buflen);
      }

      pBuffer = (const char *) pBuffer + add;
      Len    -= add;
   }

   // Process available complete blocks
   if (Len >= 64)
   {
      #if !_STRING_ARCH_unaligned
      #define UNALIGNED_P(p) ((uintptr_t) (p) % alignof (uint32_t) != 0)
         if (UNALIGNED_P (pBuffer))
         {
            while (Len > 64)
            {
               MD5ProcessBlock (pContext, memcpy (pContext->buffer, pBuffer, 64), 64);
               pBuffer = (const char *) pBuffer + 64;
               Len -= 64;
             }
         }
         else
      #endif
         {
            MD5ProcessBlock (pContext, pBuffer, Len & ~63);
            pBuffer = (const char *) pBuffer + (Len & ~63);
            Len &= 63;
         }
   }

   // Move remaining bytes in internal buffer
   if (Len > 0)
   {
      size_t left_over = pContext->buflen;

      memcpy (&((char *) pContext->buffer)[left_over], pBuffer, Len);
      left_over += Len;
      if (left_over >= 64)
      {
         MD5ProcessBlock (pContext, pContext->buffer, 64);
         left_over -= 64;
         memcpy (pContext->buffer, &pContext->buffer[16], left_over);
      }
      pContext->buflen = left_over;
   }
}

void MD5Finish (t_pMD5Context pContext, void *pDigest)
{
   // Take yet unprocessed bytes into account
   uint32_t bytes = pContext->buflen;
   size_t   size  = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

   // Count remaining bytes
   pContext->total[0] += bytes;
   if (pContext->total[0] < bytes)
      ++pContext->total[1];

   // Put the file length (in bits) at the end of the buffer
   pContext->buffer[size - 2] = SWAP  (pContext->total[0] << 3);
   pContext->buffer[size - 1] = SWAP ((pContext->total[1] << 3) | (pContext->total[0] >> 29));
   memcpy (&((char *) pContext->buffer)[bytes], MD5PadBuf, (size - 2) * 4 - bytes);

   // Process last bytes
   MD5ProcessBlock (pContext, pContext->buffer, size * 4);

   // Read digest
   char *r = (char *)pDigest;
   set_uint32 (r + 0 * sizeof pContext->A, SWAP (pContext->A));
   set_uint32 (r + 1 * sizeof pContext->B, SWAP (pContext->B));
   set_uint32 (r + 2 * sizeof pContext->C, SWAP (pContext->C));
   set_uint32 (r + 3 * sizeof pContext->D, SWAP (pContext->D));
}

#ifdef TEST
   #include <string.h>
   #include <stdlib.h>
   #include <stdio.h>

   static const char *const TestVectorArr[] =
   {
      ""                                                              ,                   // d41d8cd98f00b204e9800998ecf8427e
      "a"                                                             ,                   // 0cc175b9c0f1b6a831c399e269772661
      "abc"                                                           ,                   // 900150983cd24fb0d6963f7d28e17f72
      "message digest"                                                ,                   // f96b697d7cb7938d525a2f31aaf161d0
      "abcdefghijklmnopqrstuvwxyz"                                    ,                   // c3fcd3d76192e4007dfb496cca67e13b
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",                   // d174ab98d277d9f5a5611c2c9f419d9f
      "12345678901234567890123456789012345678901234567890123456789012345678901234567890", // 57edf4a22be3c955ac49da2e2107b67a
      nullptr
   };

   int main (int argc, char *argv[])
   {
      FILE          *f;
      int            i, j;
      char           output[65];
      t_MD5Context   Context;
      unsigned char  Buf[65536];
      unsigned char  Digest[16];
      const char   *pVector;

      if( argc < 2 )
      {
         printf( "MD5 test:\n" );

         int i=0;

         pVector = TestVectorArr[i];
         while (pVector)
         {
            int di;

            MD5Init   (&Context);
            MD5Append (&Context, pVector, strlen(pVector));
            MD5Finish (&Context, Digest);
            printf("MD5 (%-80s) = ", pVector);
            for (di = 0; di < 16; ++di)
               printf("%02x", Digest[di]);
            printf("\n");
            i++;
            pVector = TestVectorArr[i];
         }
      }
      else
      {
         if( ! ( f = fopen( argv[1], "rb" ) ) )
         {
             perror( "fopen" );
             return( 1 );
         }

         MD5Init (&Context);
         while ((i = fread (Buf, 1, sizeof(Buf), f)) > 0)
            MD5Append(&Context, Buf, i);

         MD5Finish(&Context, Digest);
         for (j = 0; j < sizeof(Digest); j++)
            printf ("%02x", Digest[j]);

         printf ("  %s\n", argv[1]);
      }
      return (0);
   }
#endif /* TEST */
