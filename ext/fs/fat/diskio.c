/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <diskio.h>		/* FatFs lower layer API */
#include <disk_access.h>
#include <ffconf.h>		/* import ff config */
#include <errno.h>

#define SYS_LOG_DOMAIN "diskio"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>

#define DISK_MAX_PHY_DRV	_VOLUMES
#define IsLower(c)		(((c)>='a')&&((c)<='z'))

/* Number of volumes (logical drives) to be used. */
const char* const disk_volume_strs[] = {
	_VOLUME_STRS
};

static struct disk_info * system_disks[DISK_MAX_PHY_DRV];

static DSTATUS translate_error(int error)
{
	switch (error) {
	case 0:
		return RES_OK;
	case EINVAL:
		return RES_PARERR;
	case ENODEV:
		return RES_NOTRDY;
	default:
		return RES_ERROR;
	}
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	struct disk_info *disk;
	int ret;

	SYS_LOG_DBG("get status pdrv %d", pdrv);

	if (pdrv >= DISK_MAX_PHY_DRV)
		return STA_NODISK;

	disk = system_disks[pdrv];
	if (!disk || !disk->op->get_status)
		return STA_NODISK;

	ret = disk->op->get_status(disk);

	return translate_error(ret);
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	struct disk_info *disk;
	int ret;

	SYS_LOG_DBG("init pdrv %d", pdrv);

	if (pdrv >= DISK_MAX_PHY_DRV)
		return STA_NODISK;

	disk = system_disks[pdrv];
	if (!disk || !disk->op->init)
		return STA_NODISK;

#ifdef CONFIG_DISKIO_CACHE
	diskio_cache_invalid(disk);
#endif

	ret = disk->op->init(disk);

	return translate_error(ret);
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,		/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	struct disk_info *disk;
	int ret;

	SYS_LOG_DBG("read pdrv %d, buff %p, sector 0x%x, count %u",
		pdrv, buff, sector, count);

	if (pdrv >= DISK_MAX_PHY_DRV)
		return STA_NODISK;

	disk = system_disks[pdrv];
	if (!disk || !disk->op->read)
		return STA_NODISK;

#ifdef CONFIG_DISKIO_CACHE
	if (diskio_is_init()) {
		ret = diskio_cache_read(disk, pdrv, buff, disk->sector_offset + sector, count);
	} else {
		ret = disk->op->read(disk, buff, disk->sector_offset + sector, count);
	}
#else
	ret = disk->op->read(disk, buff, disk->sector_offset + sector, count);
#endif
	return translate_error(ret);
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count		/* Number of sectors to write */
)
{
	struct disk_info *disk;
	int ret;

	SYS_LOG_DBG("write pdrv %d buff %p, sector 0x%x, count %u",
		pdrv, buff, sector, count);

	if (pdrv >= DISK_MAX_PHY_DRV)
		return STA_NODISK;

	disk = system_disks[pdrv];
	if (!disk || !disk->op->write)
		return STA_NODISK;

#ifdef CONFIG_DISKIO_CACHE
	if (diskio_is_init()) {
		ret = diskio_cache_write(disk, pdrv, buff, disk->sector_offset + sector, count);
	} else {
		ret = disk->op->write(disk, buff, disk->sector_offset + sector, count);
	}
#else
	ret = disk->op->write(disk, buff, disk->sector_offset + sector, count);
#endif
	return translate_error(ret);
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	struct disk_info *disk;
	int ret =  RES_OK;

	SYS_LOG_DBG("ioctl pdrv %d, cmd %u", pdrv, cmd);

	if (pdrv >= DISK_MAX_PHY_DRV)
		return STA_NODISK;

	disk = system_disks[pdrv];
	if (!disk || !disk->op->write)
		return STA_NODISK;

	switch (cmd) {
	case CTRL_SYNC:
	#ifdef CONFIG_DISKIO_CACHE
		diskio_cache_flush(disk);
	#endif
		if(disk->op->ioctl(disk, DISK_IOCTL_CTRL_SYNC, buff) != 0) {
			ret = RES_ERROR;
		}
		break;

	case GET_SECTOR_SIZE:
		if(disk->op->ioctl(disk, DISK_IOCTL_GET_SECTOR_SIZE, buff) != 0) {
			ret = RES_ERROR;
		}
		break;

	case GET_SECTOR_COUNT:
		if(disk->op->ioctl(disk, DISK_IOCTL_GET_SECTOR_COUNT, buff) != 0) {
			ret = RES_ERROR;
		}
		break;

	case GET_BLOCK_SIZE:
		if (disk->op->ioctl(disk, DISK_IOCTL_GET_ERASE_BLOCK_SZ, buff) != 0) {
			ret = RES_ERROR;
		}
		break;

	case DISK_HW_DETECT:
		if (disk->op->ioctl(disk, DISK_IOCTL_HW_DETECT, buff) != 0) {
			ret = RES_ERROR;
		}
		break;

	default:
		ret = RES_PARERR;
		break;
	}

	return ret;
}

static int vol_name_to_pdrv(const char *vol_name)
{
	const char *tp;
	const char *sp;
	int i, pdrv;
	unsigned char c, tc;

	i = 0;
	pdrv = -1;

	do {
		sp = disk_volume_strs[i];
		tp = vol_name;
		/* Compare a string drive id with path name */
		do {
			c = *sp++;
			tc = *tp++;
			if (IsLower(tc))
				tc -= 0x20;
		} while (c && (char)c == tc);

		/* Repeat for each id until pattern match */
	} while (c && ++i < DISK_MAX_PHY_DRV);

	/* If a drive id is found, get the value and strip it */
	if (i < DISK_MAX_PHY_DRV)
		pdrv = i;

	return pdrv;
}

int disk_register(struct disk_info *disk)
{
	int pdrv;

	if (!disk || !disk->name)
		return -EINVAL;

	pdrv = vol_name_to_pdrv(disk->name);
	if (pdrv < 0)
		return -EINVAL;

	SYS_LOG_INF("register disk \'%s\' pdrv %d", disk->name, pdrv);

	if (system_disks[pdrv]) {
		SYS_LOG_DBG("register: disk %s pdrv %d already registered!");
		return -EINVAL;
	}

	system_disks[pdrv] = disk;

	return 0;
}
