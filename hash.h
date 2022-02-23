// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Hash wrapper functions for uniform hash interface
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

#ifndef HASH_H
#define HASH_H

#ifndef COMMON_H
   #include "common.h"
#endif

#ifdef USE_MD5_FROM_OPENSSL
   #include <openssl/md5.h>
   typedef MD5_CTX t_HashContextMD5;
#else
   #include "md5.h"
   typedef t_MD5Context t_HashContextMD5;
#endif

#ifdef USE_SHA256_FROM_OPENSSL
   #include <openssl/sha.h>
   typedef SHA256_CTX t_HashContextSHA256;
#else
   #include "sha256.h"
   typedef t_SHA256Context  t_HashContextSHA256;
#endif

#ifdef USE_SHA1_FROM_OPENSSL
   #include <openssl/sha.h>
   typedef SHA1_CTX t_HashContextSHA1;
#else
   #include "sha1.h"
   typedef t_SHA1Context  t_HashContextSHA1;
#endif

typedef t_HashContextMD5    *t_pHashContextMD5;
typedef t_HashContextSHA1   *t_pHashContextSHA1;
typedef t_HashContextSHA256 *t_pHashContextSHA256;


class QString;

const int HASH_MD5_DIGEST_LENGTH    = MD5_DIGEST_SIZE;
const int HASH_SHA1_DIGEST_LENGTH   = SHA1_DIGEST_SIZE;
const int HASH_SHA256_DIGEST_LENGTH = SHA256_DIGEST_SIZE;

typedef struct
{
   unsigned char Buff[HASH_MD5_DIGEST_LENGTH];
} t_HashMD5Digest, *t_pHashMD5Digest;

typedef struct
{
   unsigned char Buff[HASH_SHA1_DIGEST_LENGTH];
} t_HashSHA1Digest, *t_pHashSHA1Digest;

typedef struct
{
   unsigned char Buff[HASH_SHA256_DIGEST_LENGTH];
} t_HashSHA256Digest, *t_pHashSHA256Digest;

typedef t_HashMD5Digest    const * t_pcHashMD5Digest;
typedef t_HashSHA1Digest   const * t_pcHashSHA1Digest;
typedef t_HashSHA256Digest const * t_pcHashSHA256Digest;

APIRET HashMD5Clear     (t_pHashContextMD5 pContext);
APIRET HashMD5Init      (t_pHashContextMD5 pContext);
APIRET HashMD5Append    (t_pHashContextMD5 pContext, const void *pData, unsigned int DataLen);
APIRET HashMD5Digest    (t_pHashContextMD5 pContext, t_pHashMD5Digest pDigest);
APIRET HashMD5DigestStr (t_pHashMD5Digest pDigest, QString &Str);
bool   HashMD5Match     (t_pcHashMD5Digest pDigest1, t_pcHashMD5Digest pDigest2);

APIRET HashSHA1Clear     (t_pHashContextSHA1 pContext);
APIRET HashSHA1Init      (t_pHashContextSHA1 pContext);
APIRET HashSHA1Append    (t_pHashContextSHA1 pContext, void *pData, unsigned int DataLen);
APIRET HashSHA1Digest    (t_pHashContextSHA1 pContext, t_pHashSHA1Digest pDigest);
APIRET HashSHA1DigestStr (t_pHashSHA1Digest pDigest, QString &Str);
bool   HashSHA1Match     (t_pcHashSHA1Digest pDigest1, t_pcHashSHA1Digest pDigest2);

APIRET HashSHA256Clear     (t_pHashContextSHA256 pContext);
APIRET HashSHA256Init      (t_pHashContextSHA256 pContext);
APIRET HashSHA256Append    (t_pHashContextSHA256 pContext, void *pData, unsigned int DataLen);
APIRET HashSHA256Digest    (t_pHashContextSHA256 pContext, t_pHashSHA256Digest pDigest);
APIRET HashSHA256DigestStr (t_pHashSHA256Digest pDigest, QString &Str);
bool   HashSHA256Match     (t_pcHashSHA256Digest pDigest1, t_pcHashSHA256Digest pDigest2);


APIRET HashInit   (void);
APIRET HashDeInit (void);

// ------------------------------------
//             Error codes
// ------------------------------------

enum
{
   ERROR_HASH_ = ERROR_BASE_HASH + 1,
};

#endif


