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

#include "common.h"

#include <QtCore>
#include "hash.h"

#ifdef USE_MD5_FROM_OPENSSL
#include <openssl/md5.h>
#endif

#if defined(USE_SHA256_FROM_OPENSSL) || defined(USE_SHA1_FROM_OPENSSL)
#include <openssl/sha.h>
#endif

// ------------------------------------
//             MD5 Functions
// ------------------------------------

APIRET HashMD5Clear (t_pHashContextMD5 pContext)
{
   memset (pContext, 0, sizeof(t_HashContextMD5));
   return NO_ERROR;
}

APIRET HashMD5Init (t_pHashContextMD5 pContext)
{
   #ifdef USE_MD5_FROM_OPENSSL
      (void) MD5_Init (pContext);    // strange, man page says that MD5_Init returns void, but header file says it's int
   #else
      MD5Init (pContext);
   #endif

   return NO_ERROR;
}

APIRET HashMD5Append (t_pHashContextMD5 pContext, const void *pData, unsigned int DataLen)
{
   #ifdef USE_MD5_FROM_OPENSSL
      (void) MD5_Update (pContext, pData, (unsigned long) DataLen); // Same remark as for MD5_Init
   #else
      MD5Append (pContext, pData, DataLen);
   #endif

   return NO_ERROR;
}

APIRET HashMD5Digest (t_pHashContextMD5 pContext, t_pHashMD5Digest pDigest)
{
   #ifdef USE_MD5_FROM_OPENSSL
      (void) MD5_Final (&pDigest->Buff[0], pContext); // Same remark as for MD5_Init
   #else
      MD5Finish (pContext, &pDigest->Buff[0]);
   #endif

   return NO_ERROR;
}

APIRET HashMD5DigestStr (t_pHashMD5Digest pDigest, QString &Str)
{
   bool Zero=true;

   Str.clear();
   for (int i=0; i<HASH_MD5_DIGEST_LENGTH; i++)
   {
      Str += QString ("%1") .arg ((int)(pDigest->Buff[i]), 2, 16, QChar('0'));
      if (Zero)
         Zero = (pDigest->Buff[i] == 0);
   }
   if (Zero)
      Str = "n/a";
   return NO_ERROR;
}

bool HashMD5Match (t_pcHashMD5Digest pDigest1, t_pcHashMD5Digest pDigest2)
{
   return (memcmp (pDigest1, pDigest2, sizeof(t_HashMD5Digest)) == 0);
}

// ------------------------------------
//           SHA1 Functions
// ------------------------------------

APIRET HashSHA1Clear (t_pHashContextSHA1 pContext)
{
   memset (pContext, 0, sizeof(t_HashContextSHA1));
   return NO_ERROR;
}

APIRET HashSHA1Init (t_pHashContextSHA1 pContext)
{
   #ifdef USE_SHA1_FROM_OPENSSL
      (void) SHA1_Init (pContext);
   #else
      SHA1Init (pContext);
   #endif
   return NO_ERROR;
}

APIRET HashSHA1Append (t_pHashContextSHA1 pContext, void *pData, unsigned int DataLen)
{
   #ifdef USE_SHA1_FROM_OPENSSL
      (void) SHA1_Update (pContext, pData, (unsigned long) DataLen);
   #else
      SHA1Append (pContext, (unsigned char *)pData, DataLen);
   #endif
   return NO_ERROR;
}

APIRET HashSHA1Digest (t_pHashContextSHA1 pContext, t_pHashSHA1Digest pDigest)
{
   #ifdef USE_SHA1_FROM_OPENSSL
      (void) SHA1_Final (&pDigest->Buff[0], pContext);
   #else
      SHA1Finish (pContext, (unsigned char *)pDigest);
   #endif
   return NO_ERROR;
}

APIRET HashSHA1DigestStr (t_pHashSHA1Digest pDigest, QString &Str)
{
   bool Zero = true;

   Str.clear();
   for (int i=0; i<HASH_SHA1_DIGEST_LENGTH; i++)
   {
      Str += QString ("%1") .arg ((int)(pDigest->Buff[i]), 2, 16, QChar('0'));
      if (Zero)
         Zero = (pDigest->Buff[i] == 0);
   }
   if (Zero)
      Str = "n/a";

   return NO_ERROR;
}

bool HashSHA1Match (t_pcHashSHA1Digest pDigest1, t_pcHashSHA1Digest pDigest2)
{
   return (memcmp (pDigest1, pDigest2, sizeof(t_HashSHA1Digest)) == 0);
}


// ------------------------------------
//           SHA256 Functions
// ------------------------------------

APIRET HashSHA256Clear (t_pHashContextSHA256 pContext)
{
   memset (pContext, 0, sizeof(t_HashContextSHA256));
   return NO_ERROR;
}

APIRET HashSHA256Init (t_pHashContextSHA256 pContext)
{
   #ifdef USE_SHA256_FROM_OPENSSL
      (void) SHA256_Init (pContext);
   #else
      SHA256Init (pContext);
   #endif
   return NO_ERROR;
}

APIRET HashSHA256Append (t_pHashContextSHA256 pContext, void *pData, unsigned int DataLen)
{
   #ifdef USE_SHA256_FROM_OPENSSL
      (void) SHA256_Update (pContext, pData, (unsigned long) DataLen);
   #else
      SHA256Append (pContext, (unsigned char *)pData, DataLen);
   #endif
   return NO_ERROR;
}

APIRET HashSHA256Digest (t_pHashContextSHA256 pContext, t_pHashSHA256Digest pDigest)
{
   #ifdef USE_SHA256_FROM_OPENSSL
      (void) SHA256_Final (&pDigest->Buff[0], pContext);
   #else
      SHA256Finish (pContext, (unsigned char *)pDigest);
   #endif
   return NO_ERROR;
}

APIRET HashSHA256DigestStr (t_pHashSHA256Digest pDigest, QString &Str)
{
   bool Zero = true;

   Str.clear();
   for (int i=0; i<HASH_SHA256_DIGEST_LENGTH; i++)
   {
      Str += QString ("%1") .arg ((int)(pDigest->Buff[i]), 2, 16, QChar('0'));
      if (Zero)
         Zero = (pDigest->Buff[i] == 0);
   }
   if (Zero)
      Str = "n/a";

   return NO_ERROR;
}

bool HashSHA256Match (t_pcHashSHA256Digest pDigest1, t_pcHashSHA256Digest pDigest2)
{
   return (memcmp (pDigest1, pDigest2, sizeof(t_HashMD5Digest)) == 0);
}


// ------------------------------------
//       Module initialisation
// ------------------------------------

APIRET HashInit (void)
{
   CHK (TOOL_ERROR_REGISTER_CODE (ERROR_HASH_))

   return NO_ERROR;
}

APIRET HashDeInit (void)
{
   return NO_ERROR;
}

