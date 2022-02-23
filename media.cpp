// ****************************************************************************
//  Project:        GUYMAGER
// ****************************************************************************
//  Programmer:     Guy Voncken
//                  Police Grand-Ducale
//                  Service de Police Judiciaire
//                  Section Nouvelles Technologies
// ****************************************************************************
//  Module:         Module for special media function, like HPA/DCO and others
//                  Important parts of the code found in this file have been
//                  copied from Mark Lord's famous hdparm.
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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <asm/byteorder.h>
#include <linux/hdreg.h>
#include <scsi/sg.h>

#include "common.h"
#include "media.h"

// -----------------------
//  ATA/ATAPI definitions
// -----------------------

#define MEDIA_ATAFLAGS_LBA48         0x0400
#define MEDIA_ATAFLAGS_CHECK         0xC000
#define MEDIA_ATAFLAGS_CHECK_OK      0x4000
#define MEDIA_ATAFLAGS_HPA_ENABLED   0x0400
#define MEDIA_ATAFLAGS_DCO_SUPPORTED 0x0800

typedef union
{
   unsigned short     Arr[256];             // Organised as 256 words of 16 bits each
   struct hd_driveid  Identify;
} t_MediaDriveResult;

typedef struct __attribute__ ((__packed__)) // Must be "packed" in order to have field Result on correct offset (i.e. 4)
{                                           // on any machine (otherwise, for example, the offset would be 8 on amd64)
   struct hd_drive_cmd_hdr Request;
   t_MediaDriveResult      Result;
} t_MediaDriveCmd, *t_pMediaDriveCmd;


// -------------------------------
//  Structures copied from hdparm
// -------------------------------

typedef struct
{
   t_uint8 feat;
   t_uint8 nsect;
   t_uint8 lbal;
   t_uint8 lbam;
   t_uint8 lbah;
} t_ata_lba_regs;

typedef struct
{
   t_uint8        dev;
   t_uint8        command;
   t_uint8        error;
   t_uint8        status;
   t_uint8        is_lba48;
   t_ata_lba_regs lob;
   t_ata_lba_regs hob;
} t_ata_tf;

typedef struct
{
   int            interface_id;
   int            dxfer_direction;
   unsigned char  cmd_len;
   unsigned char  mx_sb_len;
   unsigned short iovec_count;
   unsigned int   dxfer_len;
   void          *dxferp;
   unsigned char *cmdp;
   void          *sbp;
   unsigned int   timeout;
   unsigned int   flags;
   int            pack_id;
   void          *usr_ptr;
   unsigned char  status;
   unsigned char  masked_status;
   unsigned char  msg_status;
   unsigned char  sb_len_wr;
   unsigned short host_status;
   unsigned short driver_status;
   int            resid;
   unsigned int   duration;
   unsigned int   info;
} t_scsi_sg_io_hdr;

enum
{
   TASKFILE_CMD_REQ_NODATA = 0,
   TASKFILE_CMD_REQ_IN     = 2,
   TASKFILE_CMD_REQ_OUT    = 3,
   TASKFILE_CMD_REQ_RAW_OUT= 4,

   TASKFILE_DPHASE_NONE    = 0,
   TASKFILE_DPHASE_PIO_IN  = 1,
   TASKFILE_DPHASE_PIO_OUT = 4
};

typedef struct
{
   unsigned char data;
   unsigned char feat;
   unsigned char nsect;
   unsigned char lbal;
   unsigned char lbam;
   unsigned char lbah;
   unsigned char dev;
   unsigned char command;
} t_taskfile_regs;

typedef union
{
   unsigned all            : 16;
   union
   {
      unsigned lob_all     : 8;
      struct
      {
         unsigned data     : 1;
         unsigned feat     : 1;
         unsigned lbal     : 1;
         unsigned nsect    : 1;
         unsigned lbam     : 1;
         unsigned lbah     : 1;
         unsigned dev      : 1;
         unsigned command  : 1;
      } lob;
   };
   union
   {
      unsigned hob_all     : 8;
      struct
      {
         unsigned data     : 1;
         unsigned feat     : 1;
         unsigned lbal     : 1;
         unsigned nsect    : 1;
         unsigned lbam     : 1;
         unsigned lbah     : 1;
         unsigned dev      : 1;
         unsigned command  : 1;
      } hob;
   };
} t_reg_flags;

typedef struct
{
   t_taskfile_regs lob;
   t_taskfile_regs hob;
   t_reg_flags     oflags;
   t_reg_flags     iflags;
   int             dphase;
   int             cmd_req;              // IDE command_type
   unsigned long   obytes;
   unsigned long   ibytes;
   unsigned short  data[0];
} t_hdio_taskfile;

enum
{
   ATA_USING_LBA = (1 << 6),
   ATA_STAT_DRQ  = (1 << 3),
   ATA_STAT_ERR  = (1 << 0)
};

#ifndef SG_DXFER_NONE
#define SG_DXFER_NONE        -1
#define SG_DXFER_TO_DEV      -2
#define SG_DXFER_FROM_DEV    -3
#define SG_DXFER_TO_FROM_DEV -4
#endif

#define SG_READ                      0
#define SG_WRITE                     1
#define SG_PIO                       0
#define SG_DMA                       1
#define SG_ATA_16_LEN               16
#define SG_ATA_LBA48                 1
#define SG_ATA_PROTO_NON_DATA ( 3 << 1)
#define SG_ATA_PROTO_PIO_IN   ( 4 << 1)
#define SG_ATA_PROTO_PIO_OUT  ( 5 << 1)
#define SG_ATA_PROTO_DMA      ( 6 << 1)
#define SG_ATA_PROTO_UDMA_IN  (11 << 1)  // not yet supported in libata
#define SG_ATA_PROTO_UDMA_OUT (12 << 1)  // not yet supported in libata
#define SG_ATA_12                  0xa1
#define SG_ATA_16                  0x85
#define SG_ATA_12_LEN                12
#define SG_ATA_16_LEN                16
#define SG_DRIVER_SENSE            0x08
#define SG_CHECK_CONDITION         0x02

#define lba28_limit ((t_uint64)(1<<28) - 1)

enum
{
   ATA_OP_DSM                      = 0x06, // Data Set Management (TRIM)
   ATA_OP_READ_PIO                 = 0x20,
   ATA_OP_READ_PIO_ONCE            = 0x21,
   ATA_OP_READ_LONG                = 0x22,
   ATA_OP_READ_LONG_ONCE           = 0x23,
   ATA_OP_READ_PIO_EXT             = 0x24,
   ATA_OP_READ_DMA_EXT             = 0x25,
   ATA_OP_READ_FPDMA               = 0x60, // NCQ
   ATA_OP_WRITE_PIO                = 0x30,
   ATA_OP_WRITE_LONG               = 0x32,
   ATA_OP_WRITE_LONG_ONCE          = 0x33,
   ATA_OP_WRITE_PIO_EXT            = 0x34,
   ATA_OP_WRITE_DMA_EXT            = 0x35,
   ATA_OP_WRITE_FPDMA              = 0x61, // NCQ
   ATA_OP_READ_VERIFY              = 0x40,
   ATA_OP_READ_VERIFY_ONCE         = 0x41,
   ATA_OP_READ_VERIFY_EXT          = 0x42,
   ATA_OP_WRITE_UNC_EXT            = 0x45, // lba48, no data, uses feat reg
   ATA_OP_FORMAT_TRACK             = 0x50,
   ATA_OP_DOWNLOAD_MICROCODE       = 0x92,
   ATA_OP_STANDBYNOW2              = 0x94,
   ATA_OP_CHECKPOWERMODE2          = 0x98,
   ATA_OP_SLEEPNOW2                = 0x99,
   ATA_OP_PIDENTIFY                = 0xa1,
   ATA_OP_READ_NATIVE_MAX          = 0xf8,
   ATA_OP_READ_NATIVE_MAX_EXT      = 0x27,
   ATA_OP_SMART                    = 0xb0,
   ATA_OP_DCO                      = 0xb1,
   ATA_OP_ERASE_SECTORS            = 0xc0,
   ATA_OP_READ_DMA                 = 0xc8,
   ATA_OP_WRITE_DMA                = 0xca,
   ATA_OP_DOORLOCK                 = 0xde,
   ATA_OP_DOORUNLOCK               = 0xdf,
   ATA_OP_STANDBYNOW1              = 0xe0,
   ATA_OP_IDLEIMMEDIATE            = 0xe1,
   ATA_OP_SETIDLE                  = 0xe3,
   ATA_OP_SET_MAX                  = 0xf9,
   ATA_OP_SET_MAX_EXT              = 0x37,
   ATA_OP_SET_MULTIPLE             = 0xc6,
   ATA_OP_CHECKPOWERMODE1          = 0xe5,
   ATA_OP_SLEEPNOW1                = 0xe6,
   ATA_OP_FLUSHCACHE               = 0xe7,
   ATA_OP_FLUSHCACHE_EXT           = 0xea,
   ATA_OP_IDENTIFY                 = 0xec,
   ATA_OP_SETFEATURES              = 0xef,
   ATA_OP_SECURITY_SET_PASS        = 0xf1,
   ATA_OP_SECURITY_UNLOCK          = 0xf2,
   ATA_OP_SECURITY_ERASE_PREPARE   = 0xf3,
   ATA_OP_SECURITY_ERASE_UNIT      = 0xf4,
   ATA_OP_SECURITY_FREEZE_LOCK     = 0xf5,
   ATA_OP_SECURITY_DISABLE         = 0xf6
};

enum
{
   SG_CDB2_TLEN_NODATA   = 0 << 0,
   SG_CDB2_TLEN_FEAT     = 1 << 0,
   SG_CDB2_TLEN_NSECT    = 2 << 0,
   SG_CDB2_TLEN_BYTES    = 0 << 2,
   SG_CDB2_TLEN_SECTORS  = 1 << 2,
   SG_CDB2_TDIR_TO_DEV   = 0 << 3,
   SG_CDB2_TDIR_FROM_DEV = 1 << 3,
   SG_CDB2_CHECK_COND    = 1 << 5
};


// -------------------------
//  Code copied from hdparm
// -------------------------

static inline int needs_lba48 (t_uint8 ata_op, t_uint64 lba, unsigned int nsect)
{
   switch (ata_op)
   {
      case ATA_OP_DSM:
      case ATA_OP_READ_PIO_EXT:
      case ATA_OP_READ_DMA_EXT:
      case ATA_OP_WRITE_PIO_EXT:
      case ATA_OP_WRITE_DMA_EXT:
      case ATA_OP_READ_VERIFY_EXT:
      case ATA_OP_WRITE_UNC_EXT:
      case ATA_OP_READ_NATIVE_MAX_EXT:
      case ATA_OP_SET_MAX_EXT:
      case ATA_OP_FLUSHCACHE_EXT:
         return 1;
   }
   if (lba >= lba28_limit)
      return 1;
   if (nsect)
   {
      if (nsect > 0xff)
         return 1;
      if ((lba + nsect - 1) >= lba28_limit)
         return 1;
   }
   return 0;
}


static inline int is_dma (t_uint8 ata_op)
{
   switch (ata_op)
   {
      case ATA_OP_DSM:
      case ATA_OP_READ_DMA_EXT:
      case ATA_OP_READ_FPDMA:
      case ATA_OP_WRITE_DMA_EXT:
      case ATA_OP_WRITE_FPDMA:
      case ATA_OP_READ_DMA:
      case ATA_OP_WRITE_DMA:
         return SG_DMA;
      default:
         return SG_PIO;
   }
}


static void tf_init (t_ata_tf *tf, t_uint8 ata_op, t_uint64 lba, unsigned int nsect)
{
   memset(tf, 0, sizeof(*tf));
   tf->command  = ata_op;
   tf->dev      = ATA_USING_LBA;
   tf->lob.lbal = lba;
   tf->lob.lbam = lba >>  8;
   tf->lob.lbah = lba >> 16;
   tf->lob.nsect = nsect;
   if (needs_lba48(ata_op, lba, nsect))
   {
      tf->is_lba48 = 1;
      tf->hob.nsect = nsect >> 8;
      tf->hob.lbal = lba >> 24;
      tf->hob.lbam = lba >> 32;
      tf->hob.lbah = lba >> 40;
   }
   else
   {
      tf->dev |= (lba >> 24) & 0x0f;
   }
}


static t_uint64 tf_to_lba (t_ata_tf *tf)
{
   t_uint32 lba24, lbah;
   t_uint64 lba64;

   lba24 = (tf->lob.lbah << 16) | (tf->lob.lbam << 8) | (tf->lob.lbal);
   if (tf->is_lba48)
        lbah = (tf->hob.lbah << 16) | (tf->hob.lbam << 8) | (tf->hob.lbal);
   else lbah = (tf->dev & 0x0f);

   lba64 = (((t_uint64)lbah) << 24) | (t_uint64)lba24;
   return lba64;
}


static int sg16 (int fd, int rw, int dma, t_ata_tf *tf, void *data, unsigned int data_bytes, unsigned int timeout_secs)
{
   unsigned char cdb[SG_ATA_16_LEN];
   unsigned char sb[32], *desc;
   t_scsi_sg_io_hdr io_hdr;

   memset(&cdb, 0, sizeof(cdb));
   memset(&sb,     0, sizeof(sb));
   memset(&io_hdr, 0, sizeof(t_scsi_sg_io_hdr));

   if (dma)
        cdb[1] = data ? SG_ATA_PROTO_DMA : SG_ATA_PROTO_NON_DATA;
   else cdb[1] = data ? (rw ? SG_ATA_PROTO_PIO_OUT : SG_ATA_PROTO_PIO_IN) : SG_ATA_PROTO_NON_DATA;

   cdb[ 2] = SG_CDB2_CHECK_COND;
   if (data)
   {
      cdb[2] |= SG_CDB2_TLEN_NSECT | SG_CDB2_TLEN_SECTORS;
      cdb[2] |= rw ? SG_CDB2_TDIR_TO_DEV : SG_CDB2_TDIR_FROM_DEV;
   }

   //	if (!prefer_ata12 || tf->is_lba48) {
   if (tf->is_lba48)
   {
      cdb[ 0] = SG_ATA_16;
      cdb[ 4] = tf->lob.feat;
      cdb[ 6] = tf->lob.nsect;
      cdb[ 8] = tf->lob.lbal;
      cdb[10] = tf->lob.lbam;
      cdb[12] = tf->lob.lbah;
      cdb[13] = tf->dev;
      cdb[14] = tf->command;
      if (tf->is_lba48)
      {
         cdb[ 1] |= SG_ATA_LBA48;
         cdb[ 3]  = tf->hob.feat;
         cdb[ 5]  = tf->hob.nsect;
         cdb[ 7]  = tf->hob.lbal;
         cdb[ 9]  = tf->hob.lbam;
         cdb[11]  = tf->hob.lbah;
      }
      io_hdr.cmd_len = SG_ATA_16_LEN;
   }
   else
   {
      cdb[ 0] = SG_ATA_12;
      cdb[ 3] = tf->lob.feat;
      cdb[ 4] = tf->lob.nsect;
      cdb[ 5] = tf->lob.lbal;
      cdb[ 6] = tf->lob.lbam;
      cdb[ 7] = tf->lob.lbah;
      cdb[ 8] = tf->dev;
      cdb[ 9] = tf->command;
      io_hdr.cmd_len = SG_ATA_12_LEN;
   }

   io_hdr.interface_id  = 'S';
   io_hdr.mx_sb_len  = sizeof(sb);
   io_hdr.dxfer_direction  = data ? (rw ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV) : SG_DXFER_NONE;
   io_hdr.dxfer_len  = data ? data_bytes : 0;
   io_hdr.dxferp     = data;
   io_hdr.cmdp    = cdb;
   io_hdr.sbp     = sb;
   io_hdr.pack_id    = tf_to_lba(tf);
   io_hdr.timeout    = (timeout_secs ? timeout_secs : 5) * 1000; // msecs

   if (ioctl(fd, SG_IO, &io_hdr) == -1)
      return -1;                                                 // SG_IO not supported

   if (io_hdr.host_status || io_hdr.driver_status != SG_DRIVER_SENSE
      || (io_hdr.status && io_hdr.status != SG_CHECK_CONDITION))
   {
      errno = EBADE;
      return -1;
   }

   desc = sb + 8;
   if (sb[0] != 0x72 || sb[7] < 14 || desc[0] != 0x09 || desc[1] < 0x0c)
   {
      errno = EBADE;
      return -1;
   }

   tf->is_lba48  = desc[ 2] & 1;
   tf->error     = desc[ 3];
   tf->lob.nsect = desc[ 5];
   tf->lob.lbal  = desc[ 7];
   tf->lob.lbam  = desc[ 9];
   tf->lob.lbah  = desc[11];
   tf->dev       = desc[12];
   tf->status    = desc[13];
   tf->hob.feat  = 0;
   if (tf->is_lba48)
   {
      tf->hob.nsect = desc[ 4];
      tf->hob.lbal  = desc[ 6];
      tf->hob.lbam  = desc[ 8];
      tf->hob.lbah  = desc[10];
   }
   else
   {
      tf->hob.nsect = 0;
      tf->hob.lbal  = 0;
      tf->hob.lbam  = 0;
      tf->hob.lbah  = 0;
   }

   if (tf->status & (ATA_STAT_ERR | ATA_STAT_DRQ))
   {
      errno = EIO;
      return -1;
   }

   return 0;
}


static int do_taskfile_cmd (int fd, t_hdio_taskfile *r, unsigned int timeout_secs)
{
   int rc;
   t_ata_tf tf;
   void *data = nullptr;
   unsigned int data_bytes = 0;
   int rw = SG_READ;

   // Reformat and try to issue via SG_IO:
   tf_init(&tf, 0, 0, 0);

   if (r->oflags.lob.feat)    tf.lob.feat  = r->lob.feat;
   if (r->oflags.lob.lbal)    tf.lob.lbal  = r->lob.lbal;
   if (r->oflags.lob.nsect)   tf.lob.nsect = r->lob.nsect;
   if (r->oflags.lob.lbam)    tf.lob.lbam  = r->lob.lbam;
   if (r->oflags.lob.lbah)    tf.lob.lbah  = r->lob.lbah;
   if (r->oflags.lob.dev)     tf.dev       = r->lob.dev;
   if (r->oflags.lob.command) tf.command   = r->lob.command;
   if (needs_lba48(tf.command,0,0) || r->oflags.hob_all || r->iflags.hob_all)
   {
      tf.is_lba48 = 1;
      if (r->oflags.hob.feat) tf.hob.feat  = r->hob.feat;
      if (r->oflags.hob.lbal) tf.hob.lbal  = r->hob.lbal;
      if (r->oflags.hob.nsect)tf.hob.nsect = r->hob.nsect;
      if (r->oflags.hob.lbam) tf.hob.lbam  = r->hob.lbam;
      if (r->oflags.hob.lbah) tf.hob.lbah  = r->hob.lbah;
   }
   switch (r->cmd_req)
   {
      case TASKFILE_CMD_REQ_OUT:
      case TASKFILE_CMD_REQ_RAW_OUT:
         data_bytes = r->obytes;
         data       = r->data;
         rw         = SG_WRITE;
         break;
      case TASKFILE_CMD_REQ_IN:
         data_bytes = r->ibytes;
         data       = r->data;
         break;
   }

   rc = sg16(fd, rw, is_dma(tf.command), &tf, data, data_bytes, timeout_secs);
   if (rc == -1)
   {
      if (errno == EINVAL || errno == ENODEV)
         goto use_legacy_ioctl;
   }

   if (rc == 0 || errno == EIO)
   {
      if (r->iflags.lob.feat)    r->lob.feat  = tf.error;
      if (r->iflags.lob.lbal)    r->lob.lbal  = tf.lob.lbal;
      if (r->iflags.lob.nsect)   r->lob.nsect = tf.lob.nsect;
      if (r->iflags.lob.lbam)    r->lob.lbam  = tf.lob.lbam;
      if (r->iflags.lob.lbah)    r->lob.lbah  = tf.lob.lbah;
      if (r->iflags.lob.dev)     r->lob.dev   = tf.dev;
      if (r->iflags.lob.command) r->lob.command = tf.status;
      if (r->iflags.hob.feat)    r->hob.feat  = tf.hob.feat;
      if (r->iflags.hob.lbal)    r->hob.lbal  = tf.hob.lbal;
      if (r->iflags.hob.nsect)   r->hob.nsect = tf.hob.nsect;
      if (r->iflags.hob.lbam)    r->hob.lbam  = tf.hob.lbam;
      if (r->iflags.hob.lbah)    r->hob.lbah  = tf.hob.lbah;
   }

   return rc;

use_legacy_ioctl:
   errno = 0;
   rc = ioctl(fd, HDIO_DRIVE_TASKFILE, r);

   return rc;
}


static int do_drive_cmd (int fd, unsigned char *args)
{
   t_ata_tf tf;
   void *data = nullptr;
   unsigned int data_bytes = 0;
   int rc;

   if (args == nullptr)
      goto use_legacy_ioctl;

   // Reformat and try to issue via SG_IO:
   if (args[3])
   {
      data_bytes = args[3] * 512;
      data       = args + 4;
   }

   tf_init(&tf, args[0], 0, args[1]);
   tf.lob.feat = args[2];
   if (tf.command == ATA_OP_SMART)
   {
      tf.lob.nsect = args[3];
      tf.lob.lbal  = args[1];
      tf.lob.lbam  = 0x4f;
      tf.lob.lbah  = 0xc2;
   }

   rc = sg16(fd, SG_READ, is_dma(tf.command), &tf, data, data_bytes, 0);
   if (rc == -1)
   {
      if (errno == EINVAL || errno == ENODEV)
         goto use_legacy_ioctl;
   }

   if (rc == 0 || errno == EIO)
   {
      args[0] = tf.status;
      args[1] = tf.error;
      args[2] = tf.lob.nsect;
   }
   return rc;

use_legacy_ioctl:
   return ioctl(fd, HDIO_DRIVE_CMD, args);
}


static APIRET MediaIdentify (int fd, t_pMediaDriveCmd pDriveCmd)
{
   int i;

   memset(pDriveCmd, 0, sizeof(*pDriveCmd));
   pDriveCmd->Request.command      = ATA_OP_IDENTIFY;
   pDriveCmd->Request.sector_count = 1;
   if (do_drive_cmd(fd, (unsigned char *)pDriveCmd))
   {
      pDriveCmd->Request.command       = ATA_OP_PIDENTIFY;
      pDriveCmd->Request.sector_number = 0;
      pDriveCmd->Request.feature       = 0;
      pDriveCmd->Request.sector_count  = 1;
      if (do_drive_cmd (fd, (unsigned char *)pDriveCmd))
         return ERROR_MEDIA_IDENTIFY_FAILED;
   }
   /* byte-swap the little-endian IDENTIFY data to match byte-order on host CPU */
   for (i = 0; i < 0x100; ++i)
      __le16_to_cpus (&pDriveCmd->Result.Arr[i]);

   return NO_ERROR;
}


// --------------------------------
//  End of code copied from hdparm
// --------------------------------

void t_MediaInfo::InitUnknown (void)
{
   SerialNumber.clear();
   Model       .clear();
   SupportsLBA48 = t_MediaInfo::Unknown;
   SupportsHPA   = t_MediaInfo::Unknown;
   SupportsDCO   = t_MediaInfo::Unknown;
   HasHPA        = t_MediaInfo::Unknown;
   HasDCO        = t_MediaInfo::Unknown;
   HPASize       = 0;
   DCOSize       = 0;
   RealSize      = 0;
}


t_MediaInfo::t_MediaInfo ()
{
   static bool Initialised = false;

   if (!Initialised)
   {
      Initialised = true;
      CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_MEDIA_MEMALLOC_FAILED))
         CHK_EXIT (TOOL_ERROR_REGISTER_CODE (ERROR_MEDIA_IDENTIFY_FAILED))
   }

   InitUnknown ();
}


t_MediaInfo::~t_MediaInfo ()
{
}


static APIRET MediaExtractString (QString &QStr, char *pStr, int Len)
{
   char Ch;
   int  i = 0;

   while ((i+1) < Len)
   {
      Ch = pStr[i+1]; if (Ch != 0) QStr += Ch; // As stated in hdparm (see identify.c ), strings
      Ch = pStr[i  ]; if (Ch != 0) QStr += Ch; // from old devices might contain zeroes
      i += 2;
   }
   QStr = QStr.trimmed();

   return NO_ERROR;
}


static APIRET MediaGetSectorCount (int File, const char *pLinuxDevice, bool SupportsLBA48, t_uint64 *pSectorCountDeviceConfigIdentify, t_uint64 *pSectorCountReadNativeMax)
{
   t_MediaDriveCmd Cmd;
   int             rc=0;
   t_hdio_taskfile r;

   *pSectorCountDeviceConfigIdentify = 0;
   *pSectorCountReadNativeMax        = 0;

   // ATA command: Device Configuration Identity
   // ------------------------------------------
   memset (&Cmd, 0, (sizeof(Cmd)));
   Cmd.Request.command = 0xb1;  Cmd.Request.sector_number = 0;
   Cmd.Request.feature = 0xc2;  Cmd.Request.sector_count  = 1;
   rc = ioctl(File, HDIO_DRIVE_CMD, &Cmd);
   if (rc)
        LOG_INFO ("ATA command DEVICE_CONFIGURATION_IDENTITY failed for device %s (%d / %d)", pLinuxDevice, rc, errno)
   else *pSectorCountDeviceConfigIdentify = 1 + *((t_uint64 *)(&Cmd.Result.Arr[3]));

   // ATA command: Read Native Max Address (Ext)
   // ------------------------------------------
   memset(&r, 0, sizeof(r));
   r.cmd_req = TASKFILE_CMD_REQ_NODATA;
   r.dphase  = TASKFILE_DPHASE_NONE;
   r.oflags.lob.dev     = 1;  r.iflags.lob.lbal = 1;  r.iflags.hob.lbal  = 1;
   r.oflags.lob.command = 1;  r.iflags.lob.lbam = 1;  r.iflags.hob.lbam  = 1;
   r.iflags.lob.command = 1;  r.iflags.lob.lbah = 1;  r.iflags.hob.lbah  = 1;
   r.lob.dev     = 0x40;
   r.lob.command = ATA_OP_READ_NATIVE_MAX_EXT;
   rc = do_taskfile_cmd (File, &r, 10);
   if (rc)
   {
      LOG_INFO ("ATA command READ_NATIVE_MAX_ADDRESS_EXT failed for device %s (%d / %d)", pLinuxDevice, rc, errno)
      if (!SupportsLBA48) // If the device supports LBA48, then READ_NATIVE_MAX_ADDRESS_EXT should have work, so, only try READ_NATIVE_MAX_ADDRESS if LBA48 is not supported.
      {
         LOG_INFO ("LBA48 is not supported, retrying with READ_NATIVE_MAX_ADDRESS")
         memset(&r, 0, sizeof(r));
         r.cmd_req = TASKFILE_CMD_REQ_NODATA;
         r.dphase  = TASKFILE_DPHASE_NONE;
         r.oflags.lob.dev     = 1;  r.iflags.lob.lbal = 1;
         r.oflags.lob.command = 1;  r.iflags.lob.lbam = 1;
         r.iflags.lob.command = 1;  r.iflags.lob.lbah = 1;
         r.iflags.lob.dev     = 1;
         r.lob.dev            = 0x40;
         r.lob.command        = WIN_READ_NATIVE_MAX;
         rc = do_taskfile_cmd (File, &r, 10);
         if (rc)
              LOG_INFO ("ATA command READ_NATIVE_MAX_ADDRESS failed for device %s (%d / %d)", pLinuxDevice, rc, errno)
         else *pSectorCountReadNativeMax = (((r.lob.dev & 0x0F) << 24) | (r.lob.lbah << 16) |
                                             (r.lob.lbam        <<  8) |  r.lob.lbal      ) + 1;
      }
   }
   else
   {
      *pSectorCountReadNativeMax = ((((t_uint64) r.hob.lbah) << 40) | (((t_uint64) r.hob.lbam) << 32) |
                                    (((t_uint64) r.hob.lbal) << 24) | (((t_uint64) r.lob.lbah) << 16) |
                                    (((t_uint64) r.lob.lbam) << 8 ) | (((t_uint64) r.lob.lbal)      )) +1;
   }

   return NO_ERROR;
}


APIRET t_MediaInfo::QueryDevice (const char *pLinuxDevice)
{
   t_pMediaDriveCmd   pMediaDriveCmdATA;
   APIRET              rc;
   int                 File;
   struct hd_driveid *pIdentify;

   // Init and submit ATA command IDENTIFY DEVICE
   // -------------------------------------------
   InitUnknown();

   pMediaDriveCmdATA   = (t_pMediaDriveCmd) malloc (sizeof(t_MediaDriveCmd));
   memset (pMediaDriveCmdATA, 0, sizeof(t_MediaDriveCmd));

   File = open (pLinuxDevice, O_RDONLY);
   if (File < 0)
   {
      LOG_INFO ("open failed for %s (%d)", pLinuxDevice, File);
      return NO_ERROR;
   }

   // Extract result
   // --------------
   rc = MediaIdentify (File, pMediaDriveCmdATA);
   if (rc == ERROR_MEDIA_IDENTIFY_FAILED)
   {
      LOG_INFO ("ioctl identify failed for %s", pLinuxDevice);
   }
   else
   {
      CHK (rc)
      t_uint64 SectorCountIdentifyDevice;
      t_uint64 SectorCountDeviceConfigIdentify;
      t_uint64 SectorCountReadNativeMax;

      pIdentify = &pMediaDriveCmdATA->Result.Identify;
      SectorCountIdentifyDevice = GETMAX (pIdentify->lba_capacity, pIdentify->lba_capacity_2);

      CHK (MediaExtractString (SerialNumber, (char*)pIdentify->serial_no, sizeof(pIdentify->serial_no)))
      CHK (MediaExtractString (Model       , (char*)pIdentify->model    , sizeof(pIdentify->model    )))

      #define FLAG(Val, Flag) (((Val) & (Flag)) == (Flag)) ? t_MediaInfo::Yes : t_MediaInfo::No
      SupportsLBA48 = FLAG(pIdentify->command_set_2, MEDIA_ATAFLAGS_LBA48 | MEDIA_ATAFLAGS_CHECK) ;
      SupportsHPA   = FLAG(pIdentify->cfs_enable_1 , MEDIA_ATAFLAGS_HPA_ENABLED  );
      SupportsDCO   = FLAG(pIdentify->command_set_2, MEDIA_ATAFLAGS_DCO_SUPPORTED);
      #undef FLAG

      CHK (MediaGetSectorCount (File, pLinuxDevice, SupportsLBA48, &SectorCountDeviceConfigIdentify, &SectorCountReadNativeMax))

      LOG_INFO ("%s model and serial number: [%s] [%s]", pLinuxDevice, QSTR_TO_PSZ(Model), QSTR_TO_PSZ(SerialNumber))
      LOG_INFO ("%s sectors->IdentifyDevice: %llu"     , pLinuxDevice, SectorCountIdentifyDevice      )
      LOG_INFO ("%s sectors->ReadNativeMax : %llu"     , pLinuxDevice, SectorCountReadNativeMax       )
      LOG_INFO ("%s sectors->ConfigIdentify: %llu"     , pLinuxDevice, SectorCountDeviceConfigIdentify)
//      printf ("\n[%s] [%s] [%s]", pLinuxDevice, QSTR_TO_PSZ(Model), QSTR_TO_PSZ(SerialNumber));
//      printf ("\nIdentify       %16llu %16llX", SectorCountIdentifyDevice      , SectorCountIdentifyDevice      );
//      printf ("\nNative         %16llu %16llX", SectorCountReadNativeMax       , SectorCountReadNativeMax       );
//      printf ("\nConfigIdentify %16llu %16llX", SectorCountDeviceConfigIdentify, SectorCountDeviceConfigIdentify);

      if (SupportsHPA)
      {
         if      (SectorCountReadNativeMax == 0                        ) HasHPA = t_MediaInfo::Unknown;
         else if (SectorCountReadNativeMax >  SectorCountIdentifyDevice) HasHPA = t_MediaInfo::Yes;
         else if (SectorCountReadNativeMax == SectorCountIdentifyDevice) HasHPA = t_MediaInfo::No;
         else
         {
            LOG_INFO ("Strange, sector count NativeMax less than IdentifyDevice: %s %llu < %llu", pLinuxDevice, SectorCountReadNativeMax, SectorCountIdentifyDevice)
               HasHPA = t_MediaInfo::Unknown;
            SectorCountReadNativeMax = 0;
         }
      }
      else                         // Maybe the media supports HPA, but the feature has been switched off by means
      {                            // of DCO. In that case the flag MediaInfo.SupportsHPA is wrong, but that doesn't
         HasHPA = t_MediaInfo::No; // make problems, as we only are interested if there's a HPA or not. If HPA
      }                            // has been disabled by DCO, we know that no HPA has been set on the media.

      if (SupportsDCO)
      {
         if      ((SectorCountDeviceConfigIdentify == 0) || (SectorCountReadNativeMax == 0)) HasDCO = t_MediaInfo::Unknown;
         else if  (SectorCountDeviceConfigIdentify >  SectorCountReadNativeMax)              HasDCO = t_MediaInfo::Yes;
         else if  (SectorCountDeviceConfigIdentify == SectorCountReadNativeMax)              HasDCO = t_MediaInfo::No;
         else
         {
            LOG_INFO ("Strange, sector count DeviceConfigIdentify less than NativeMax: %s %llu < %llu", pLinuxDevice, SectorCountDeviceConfigIdentify, SectorCountReadNativeMax)
            HasDCO = t_MediaInfo::Unknown;
            SectorCountDeviceConfigIdentify = 0;
         }
      }
      else
      {
         HasDCO = t_MediaInfo::No;
      }
   }

   // Free resources
   // --------------
   if (close (File) != 0)
      LOG_INFO ("Close failed for %s", pLinuxDevice);

   free (pMediaDriveCmdATA);

   return NO_ERROR;
}

