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

// See also header in sha256.cpp.

#ifndef SHA256_H
#define SHA256_H

typedef unsigned int  uint32;
typedef unsigned char uint8;

#define SHA256_DIGEST_SIZE 32

#ifdef SHA256_OLD
   typedef struct
   {
      uint32 total[2];
      uint32 state[8];
      uint8 buffer[64];
   } t_SHA256Context, *t_pSHA256Context;

   void SHA256Init   (t_pSHA256Context pContext);
   void SHA256Append (t_pSHA256Context pContext, uint8 *input, uint32 length );
   void SHA256Finish (t_pSHA256Context pContext, uint8 digest[32] );
#else
   typedef struct
   {
      uint32 state[8];
      uint32 total[2];
      size_t buflen;
      uint32 buffer[32];
   } t_SHA256Context, *t_pSHA256Context;

   // Always make sure that these functions are compiled with O3 optimisation or
   // else, performance is about 5 times worse! See also SHA256ProcessBlock in sha256.cpp.
//   void SHA256Init   (t_pSHA256Context pContext)                                 __attribute__((optimize("-O3")));
//   void SHA256Append (t_pSHA256Context pContext, const void *buffer, size_t len) __attribute__((optimize("-O3")));
//   void SHA256Finish (t_pSHA256Context pContext, void *pDigest)                  __attribute__((optimize("-O3")));

   void SHA256Init   (t_pSHA256Context pContext);
   void SHA256Append (t_pSHA256Context pContext, const void *buffer, size_t len);
   void SHA256Finish (t_pSHA256Context pContext, void *pDigest);
#endif

#endif

