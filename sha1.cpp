// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         SHA1 calculation
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

// This module is based on sha1sum (see Debian package coreutils, for example).
// The core functions have been mainly copy/pasted; they have been written
// by Scott G. Miller and are copyrighted by the Free Software Foundation, Inc.


#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "sha1.h"


#ifdef WORDS_BIGENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) \
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#endif

static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };  // This array contains the bytes used to pad the buffer to
                                                                          // the next 64-byte boundary.  (RFC 1321, 3.1: Step 1)

// SHA1 round constants
#define K1 0x5a827999
#define K2 0x6ed9eba1
#define K3 0x8f1bbcdc
#define K4 0xca62c1d6

// Round functions.
#define F1(B,C,D) ( D ^ ( B & ( C ^ D ) ) )
#define F2(B,C,D) (B ^ C ^ D)
#define F3(B,C,D) ( ( B & C ) | ( D & ( B | C ) ) )
#define F4(B,C,D) (B ^ C ^ D)


void sha1_process_block (const void *buffer, size_t len, t_pSHA1Context pContext)
{
   const uint32_t *words  = (uint32_t *) buffer;
   size_t          nwords = len / sizeof (uint32_t);
   const uint32_t *endp   = words + nwords;
   uint32_t x[16];
   uint32_t a = pContext->A;
   uint32_t b = pContext->B;
   uint32_t c = pContext->C;
   uint32_t d = pContext->D;
   uint32_t e = pContext->E;

  // First increment the byte count.  RFC 1321 specifies the possible length of the file
  // up to 2^64 bits.  Here we only compute the number of bytes.  Do a double word increment.
   pContext->total[0] += len;
   if (pContext->total[0] < len)
      ++pContext->total[1];

   #define rol(x, n) (((x) << (n)) | ((uint32_t) (x) >> (32 - (n))))

   #define M(I) ( tm =  x[I    &0x0f] ^ x[(I-14)&0x0f] \
                     ^  x[(I-8)&0x0f] ^ x[(I- 3)&0x0f] \
                     , (x[I    &0x0f] = rol(tm, 1)))

   #define R(A,B,C,D,E,F,K,M)  do { E += rol( A, 5 )     \
                                         + F( B, C, D )  \
                                         + K             \
                                         + M;            \
                                    B  = rol( B, 30 );   \
                                  } while(0)

   while (words < endp)
   {
      uint32_t tm;
      int      t;

      for (t = 0; t < 16; t++)
      {
         x[t] = SWAP (*words);
         words++;
      }

      R( a, b, c, d, e, F1, K1, x[ 0] );
      R( e, a, b, c, d, F1, K1, x[ 1] );
      R( d, e, a, b, c, F1, K1, x[ 2] );
      R( c, d, e, a, b, F1, K1, x[ 3] );
      R( b, c, d, e, a, F1, K1, x[ 4] );
      R( a, b, c, d, e, F1, K1, x[ 5] );
      R( e, a, b, c, d, F1, K1, x[ 6] );
      R( d, e, a, b, c, F1, K1, x[ 7] );
      R( c, d, e, a, b, F1, K1, x[ 8] );
      R( b, c, d, e, a, F1, K1, x[ 9] );
      R( a, b, c, d, e, F1, K1, x[10] );
      R( e, a, b, c, d, F1, K1, x[11] );
      R( d, e, a, b, c, F1, K1, x[12] );
      R( c, d, e, a, b, F1, K1, x[13] );
      R( b, c, d, e, a, F1, K1, x[14] );
      R( a, b, c, d, e, F1, K1, x[15] );
      R( e, a, b, c, d, F1, K1, M(16) );
      R( d, e, a, b, c, F1, K1, M(17) );
      R( c, d, e, a, b, F1, K1, M(18) );
      R( b, c, d, e, a, F1, K1, M(19) );
      R( a, b, c, d, e, F2, K2, M(20) );
      R( e, a, b, c, d, F2, K2, M(21) );
      R( d, e, a, b, c, F2, K2, M(22) );
      R( c, d, e, a, b, F2, K2, M(23) );
      R( b, c, d, e, a, F2, K2, M(24) );
      R( a, b, c, d, e, F2, K2, M(25) );
      R( e, a, b, c, d, F2, K2, M(26) );
      R( d, e, a, b, c, F2, K2, M(27) );
      R( c, d, e, a, b, F2, K2, M(28) );
      R( b, c, d, e, a, F2, K2, M(29) );
      R( a, b, c, d, e, F2, K2, M(30) );
      R( e, a, b, c, d, F2, K2, M(31) );
      R( d, e, a, b, c, F2, K2, M(32) );
      R( c, d, e, a, b, F2, K2, M(33) );
      R( b, c, d, e, a, F2, K2, M(34) );
      R( a, b, c, d, e, F2, K2, M(35) );
      R( e, a, b, c, d, F2, K2, M(36) );
      R( d, e, a, b, c, F2, K2, M(37) );
      R( c, d, e, a, b, F2, K2, M(38) );
      R( b, c, d, e, a, F2, K2, M(39) );
      R( a, b, c, d, e, F3, K3, M(40) );
      R( e, a, b, c, d, F3, K3, M(41) );
      R( d, e, a, b, c, F3, K3, M(42) );
      R( c, d, e, a, b, F3, K3, M(43) );
      R( b, c, d, e, a, F3, K3, M(44) );
      R( a, b, c, d, e, F3, K3, M(45) );
      R( e, a, b, c, d, F3, K3, M(46) );
      R( d, e, a, b, c, F3, K3, M(47) );
      R( c, d, e, a, b, F3, K3, M(48) );
      R( b, c, d, e, a, F3, K3, M(49) );
      R( a, b, c, d, e, F3, K3, M(50) );
      R( e, a, b, c, d, F3, K3, M(51) );
      R( d, e, a, b, c, F3, K3, M(52) );
      R( c, d, e, a, b, F3, K3, M(53) );
      R( b, c, d, e, a, F3, K3, M(54) );
      R( a, b, c, d, e, F3, K3, M(55) );
      R( e, a, b, c, d, F3, K3, M(56) );
      R( d, e, a, b, c, F3, K3, M(57) );
      R( c, d, e, a, b, F3, K3, M(58) );
      R( b, c, d, e, a, F3, K3, M(59) );
      R( a, b, c, d, e, F4, K4, M(60) );
      R( e, a, b, c, d, F4, K4, M(61) );
      R( d, e, a, b, c, F4, K4, M(62) );
      R( c, d, e, a, b, F4, K4, M(63) );
      R( b, c, d, e, a, F4, K4, M(64) );
      R( a, b, c, d, e, F4, K4, M(65) );
      R( e, a, b, c, d, F4, K4, M(66) );
      R( d, e, a, b, c, F4, K4, M(67) );
      R( c, d, e, a, b, F4, K4, M(68) );
      R( b, c, d, e, a, F4, K4, M(69) );
      R( a, b, c, d, e, F4, K4, M(70) );
      R( e, a, b, c, d, F4, K4, M(71) );
      R( d, e, a, b, c, F4, K4, M(72) );
      R( c, d, e, a, b, F4, K4, M(73) );
      R( b, c, d, e, a, F4, K4, M(74) );
      R( a, b, c, d, e, F4, K4, M(75) );
      R( e, a, b, c, d, F4, K4, M(76) );
      R( d, e, a, b, c, F4, K4, M(77) );
      R( c, d, e, a, b, F4, K4, M(78) );
      R( b, c, d, e, a, F4, K4, M(79) );

      a = pContext->A += a;
      b = pContext->B += b;
      c = pContext->C += c;
      d = pContext->D += d;
      e = pContext->E += e;
   }
}

void SHA1Init (t_pSHA1Context pContext)
{
   pContext->A = 0x67452301;
   pContext->B = 0xefcdab89;
   pContext->C = 0x98badcfe;
   pContext->D = 0x10325476;
   pContext->E = 0xc3d2e1f0;

   pContext->total[0] = pContext->total[1] = 0;
   pContext->buflen   = 0;
}

static inline void set_uint32 (char *cp, uint32_t v)
{
   memcpy (cp, &v, sizeof v);
}

void SHA1Finish (t_pSHA1Context pContext, void *pDigest)
{
   uint32_t bytes = pContext->buflen;                        // Take yet unprocessed bytes into account.
   size_t   size  = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

   pContext->total[0] += bytes;                              // Now count remaining bytes.
   if (pContext->total[0] < bytes)
      ++pContext->total[1];

   pContext->buffer[size - 2] = SWAP ((pContext->total[1] << 3) | (pContext->total[0] >> 29)); // Put the 64-bit file length in *bits* at the end of the buffer.
   pContext->buffer[size - 1] = SWAP  (pContext->total[0] << 3);

   memcpy (&((char *) pContext->buffer)[bytes], fillbuf, (size - 2) * 4 - bytes);

   sha1_process_block (pContext->buffer, size * 4, pContext);     // Process last bytes.

   char *r = (char *) pDigest;
   set_uint32 (r + 0 * sizeof pContext->A, SWAP (pContext->A));
   set_uint32 (r + 1 * sizeof pContext->B, SWAP (pContext->B));
   set_uint32 (r + 2 * sizeof pContext->C, SWAP (pContext->C));
   set_uint32 (r + 3 * sizeof pContext->D, SWAP (pContext->D));
   set_uint32 (r + 4 * sizeof pContext->E, SWAP (pContext->E));
}


void SHA1Append (t_pSHA1Context pContext, const void *buffer, size_t len)
{
   // When we already have some bits in our internal buffer concatenate both inputs first.
   if (pContext->buflen != 0)
   {
      size_t left_over = pContext->buflen;
      size_t add       = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&((char *) pContext->buffer)[left_over], buffer, add);
      pContext->buflen += add;

      if (pContext->buflen > 64)
      {
         sha1_process_block (pContext->buffer, pContext->buflen & ~63, pContext);

         pContext->buflen &= 63;
         /* The regions in the following copy operation cannot overlap.  */
         memcpy (pContext->buffer,
                 &((char *) pContext->buffer)[(left_over + add) & ~63],
                 pContext->buflen);
      }
      buffer = (const char *) buffer + add;
      len -= add;
    }

   // Process available complete blocks.
   if (len >= 64)
   {
      #if !_STRING_ARCH_unaligned
         #define UNALIGNED_P(p) ((uintptr_t) (p) % alignof (uint32_t) != 0)
         if (UNALIGNED_P (buffer))
            while (len > 64)
            {
               sha1_process_block (memcpy (pContext->buffer, buffer, 64), 64, pContext);
               buffer = (const char *) buffer + 64;
               len -= 64;
            }
         else
      #endif
         {
            sha1_process_block (buffer, len & ~63, pContext);
            buffer = (const char *) buffer + (len & ~63);
            len &= 63;
         }
   }

   // Move remaining bytes in internal buffer.
   if (len > 0)
   {
      size_t left_over = pContext->buflen;

      memcpy (&((char *) pContext->buffer)[left_over], buffer, len);
      left_over += len;
      if (left_over >= 64)
      {
         sha1_process_block (pContext->buffer, 64, pContext);
         left_over -= 64;
         memcpy (pContext->buffer, &pContext->buffer[16], left_over);
      }
      pContext->buflen = left_over;
   }
}

