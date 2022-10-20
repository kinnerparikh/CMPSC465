#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int is_mounted = 0;
int is_written = 0;

uint32_t op(uint32_t diskId, uint32_t blockId, uint32_t command, uint32_t reserved)
{
	return (command << 12 | diskId << 8 | blockId | reserved << 18);
}

int mdadm_mount(void)
{
	if (is_mounted == 0)
	{
		jbod_operation(op(0, 0, JBOD_MOUNT, 0), NULL);
		is_mounted = 1;
		return 1;
	}
	return -1;
}

int mdadm_unmount(void) 
{
	if (is_mounted == 1)
	{
		jbod_operation(op(0, 0, JBOD_UNMOUNT, 0), NULL);
		is_mounted = 0;
		return 1;
	}
	return -1;
}

int mdadm_write_permission(void)
{
	if (is_written == 0)
	{
		jbod_operation(op(0, 0, JBOD_WRITE_PERMISSION, 0), NULL);
		is_written = 1;
		return 1;
	}
	return -1;
}


int mdadm_revoke_write_permission(void)
{
	if (is_written == 1)
	{
		jbod_operation(op(0, 0, JBOD_REVOKE_WRITE_PERMISSION, 0), NULL);
		is_written = 0;
		return 1;
	}
	return -1;
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)
{
  	int status = jbod_operation(op(0, 0, JBOD_SEEK_TO_DISK, 0), NULL);
	// checks if unmounted, read is too large, wants to read past total storage
	if (status != 0 || read_len > 2048 || start_addr + read_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS || (read_buf == NULL && read_len != 0))
	{
		return -1;
	}
	uint32_t curr_addr = start_addr;
	uint32_t tot_bits_read = 0;

	// gets current disk and current block
	uint32_t curr_disk = curr_addr / JBOD_DISK_SIZE;
	jbod_operation(op(curr_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
	jbod_operation(op(0, ((curr_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE), JBOD_SEEK_TO_BLOCK, 0), NULL);

	while (tot_bits_read < read_len)
	{
		// reads the block onto curr_buf
		uint8_t curr_buf[JBOD_BLOCK_SIZE];
		jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), curr_buf);
		
		int max_addr_in_block = JBOD_BLOCK_SIZE - ((curr_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE);

		//uint8_t *dest = read_buf + tot_bits_read;
		uint8_t *src = curr_buf;
		uint32_t n = read_len;
		
		// checks if the current block will exceed the read length
		if (curr_addr + JBOD_BLOCK_SIZE <= start_addr + read_len)
		{
			src += JBOD_BLOCK_SIZE - max_addr_in_block;
			n = max_addr_in_block;
		}
		else
		{
			n -= tot_bits_read;
		}
		// copies desired bits from curr_buf into read_buf
		memcpy(read_buf + tot_bits_read, src, n);
		tot_bits_read += max_addr_in_block;
		curr_addr = start_addr + tot_bits_read;

		// increments disk if needed
		if (curr_addr / JBOD_DISK_SIZE != curr_disk)
		{
			curr_disk = curr_addr / JBOD_DISK_SIZE;
			jbod_operation(op(curr_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
		}
	}
	return read_len;
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf)
{
	if (is_mounted == 0 || is_written == 0 || write_len > 2048 || start_addr + write_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS || (write_buf == NULL && write_len != 0))
	{
		return -1;
	}
	uint32_t curr_addr = start_addr;
	uint32_t tot_bits_written = 0;


	/*
	uint8_t curr_buf[JBOD_BLOCK_SIZE];
	jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), curr_buf);

	uint32_t curr_point = curr_addr % JBOD_DISK_SIZE % JBOD_BLOCK_SIZE;
	if (curr_point + write_len > 256) return -1;
	memcpy(curr_buf + curr_point, write_buf, write_len);
	
	jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), curr_buf);
*/
	
	while (tot_bits_written < write_len)
	{
		uint32_t curr_disk = curr_addr / JBOD_DISK_SIZE;
		jbod_operation(op(curr_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
		jbod_operation(op(0, ((curr_addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE), JBOD_SEEK_TO_BLOCK, 0), NULL);
		
		uint8_t curr_buf[JBOD_BLOCK_SIZE];
		jbod_operation(op(0, 0, JBOD_READ_BLOCK, 0), curr_buf);

		//int max_addr_in_block = JBOD_BLOCK_SIZE - ((curr_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE);
		uint32_t left_in_block = (curr_addr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
		

		/*
		uint8_t *dest = curr_buf;
		//uint8_t *src = write_buf + tot_bits_written;
		uint32_t n = write_len;
		if (curr_addr + JBOD_BLOCK_SIZE <= start_addr + write_len)
		{
			dest += JBOD_BLOCK_SIZE - max_addr_in_block;
			n = max_addr_in_block;
		}
		else
		{
			n -= tot_bits_written;
		}
		memcpy(dest, write_buf + tot_bits_written, n);
		jbod_operation(op(0, 0, JBOD_WRITE_BLOCK, 0), curr_buf);

		tot_bits_written += max_addr_in_block;
		curr_addr = start_addr + tot_bits_written;

		if (curr_addr / JBOD_DISK_SIZE != curr_disk)
		{
			curr_disk = curr_addr / JBOD_DISK_SIZE;
			jbod_operation(op(curr_disk, 0, JBOD_SEEK_TO_DISK, 0), NULL);
		}
		*/

	}
	


	return write_len;
}

