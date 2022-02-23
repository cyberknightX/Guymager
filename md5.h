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

// The core functions of this module are based on the functions found in
// standard Linux' md5sum command, contained in package coreutils on Debian.

#ifndef MD5_H
#define MD5_H

#define MD5_DIGEST_SIZE 16
#define MD5_BLOCK_SIZE  64

typedef unsigned int uint32_t;

typedef struct
{
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;

  uint32_t total[2];
  uint32_t buflen;
  uint32_t buffer[32];
} t_MD5Context, *t_pMD5Context;

// Always make sure that these functions are compiled with O3 optimisation or
// else, performance is about 5 times worse! See also MD5ProcessBlock in md5.cpp.

//void MD5Init   (t_pMD5Context pContext)                                   __attribute__((optimize("-O3")));
//void MD5Append (t_pMD5Context pContext, const void *pBuffer, size_t Len)  __attribute__((optimize("-O3")));
//void MD5Finish (t_pMD5Context pContext, void *pDigest)                    __attribute__((optimize("-O3")));

void MD5Init   (t_pMD5Context pContext);
void MD5Append (t_pMD5Context pContext, const void *pBuffer, size_t Len);
void MD5Finish (t_pMD5Context pContext, void *pDigest);

#endif
