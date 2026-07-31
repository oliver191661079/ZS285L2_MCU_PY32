#include <sdk.h>
#include <spi/spinor.h>

int fwinfo_read(void* buf, unsigned int offset, unsigned int size)
{
	void *tmp_buf;
	int sector_num;

	if(offset + size > (BLOCK0_FWINFO_SECTOR_NUM * 512))
		return -EPERM;

	sector_num = (sizeof(fwinfo_t) + 512 - 1) / 512;
	tmp_buf = malloc(sector_num * 512);
	if(tmp_buf == NULL)
		return -ENOMEM;

	spinor_read(BLOCK0_FWINFO_SECTOR, tmp_buf, 1, 1);
	if(*((unsigned int*)tmp_buf) != 0x4E495746)
	{
		free(tmp_buf);
		return -ENOENT;
	}
	if(sector_num > 1)
		spinor_read(BLOCK0_FWINFO_SECTOR + 1, tmp_buf + 512, sector_num - 1, 1);

	memcpy(buf, tmp_buf + offset, size);
	free(tmp_buf);

	return 0;
}

