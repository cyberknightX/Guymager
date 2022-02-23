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

// See also header in sha1.cpp.

#ifndef SHA1_H
#define SHA1_H

#define SHA1_DIGEST_SIZE 20

typedef unsigned int uint32_t;

/* Structure to save state of computation between the single steps.  */
typedef struct
{
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;
  uint32_t E;

  uint32_t total[2];
  uint32_t buflen;
  uint32_t buffer[32];
} t_SHA1Context, *t_pSHA1Context;

void SHA1Init   (t_pSHA1Context pContext);
void SHA1Finish (t_pSHA1Context pContext, void *pDigest);
void SHA1Append (t_pSHA1Context pContext, const void *buffer, size_t len);

#endif
