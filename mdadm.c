#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"
#include "cache.h"

static int mounted = -1;

// Formatting helper function
static uint32_t cmd_code(jbod_cmd_t cmd, uint8_t diskID, uint16_t rsvd, uint8_t blockID)
{
  return (cmd << 26) | (diskID << 22) | (rsvd << 8) | blockID;
}

int mdadm_mount(void)
{
  uint32_t op = cmd_code(JBOD_MOUNT, 0, 0, 0);
  int m = jbod_client_operation(op, NULL);

  if (m == 0)
  {
    mounted = 1;
  }

  return (m == 0) ? 1 : -1;
}

int mdadm_unmount(void)
{
  uint32_t op = cmd_code(JBOD_UNMOUNT, 0, 0, 0);
  int um = jbod_client_operation(op, NULL);

  if (um == 0)
  {
    mounted = 0;
  }

  return (um == 0) ? 1 : -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  if (!mounted || len > 1024 || len + addr > 1048575 || (buf == NULL && len > 0))
  {
    return -1;
  }

  size_t totalBytesRead = 0;

  // Loops until all bytes are read
  while (totalBytesRead < len)
  {
    int diskID = addr / JBOD_DISK_SIZE;
    int blockID = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int blockOffset = addr % JBOD_BLOCK_SIZE;

    // Create a temporary buffer to store the data read
    uint8_t temp[JBOD_BLOCK_SIZE];

    // Seek the disk
    uint32_t op = cmd_code(JBOD_SEEK_TO_DISK, diskID, 0, 0);
    jbod_client_operation(op, NULL);

    // Seek the block
    op = cmd_code(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
    jbod_client_operation(op, NULL);

    // Read block
    op = cmd_code(JBOD_READ_BLOCK, 0, 0, 0);
    jbod_client_operation(op, temp);

    // Calculates number of bytes to read from current block
    size_t bytesToRead = (len - totalBytesRead > JBOD_BLOCK_SIZE - blockOffset) ? JBOD_BLOCK_SIZE - blockOffset : len - totalBytesRead;

    // Copy read data from temporary buffer to output buffer
    memcpy(buf + totalBytesRead, temp + blockOffset, bytesToRead);

    // Update number of bytes read and current address
    totalBytesRead += bytesToRead;
    addr += bytesToRead;
  }

  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
  if (!mounted || len > 1024 || len + addr > 1048576 || (buf == NULL && len > 0))
  {
    return -1;
  }

  size_t totalBytesWritten = 0;
  uint8_t temp[JBOD_BLOCK_SIZE];

  while (totalBytesWritten < len)
  {
    int diskID = addr / JBOD_DISK_SIZE;
    int blockID = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    int blockOffset = addr % JBOD_BLOCK_SIZE;

    // Calculate number of bytes to write to current block
    size_t bytesToWrite = (len - totalBytesWritten > JBOD_BLOCK_SIZE - blockOffset) ? JBOD_BLOCK_SIZE - blockOffset : len - totalBytesWritten;

    // Seek the disk
    uint32_t op = cmd_code(JBOD_SEEK_TO_DISK, diskID, 0, 0);
    jbod_client_operation(op, NULL);

    // Seek the block
    op = cmd_code(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
    jbod_client_operation(op, NULL);

    // Read block
    op = cmd_code(JBOD_READ_BLOCK, 0, 0, 0);
    jbod_client_operation(op, temp);

    // Copy data to be written to the current block
    memcpy(temp + blockOffset, buf + totalBytesWritten, bytesToWrite);

    // Write the current block
    op = cmd_code(JBOD_SEEK_TO_DISK, diskID, 0, 0);
    jbod_client_operation(op, NULL);

    op = cmd_code(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
    jbod_client_operation(op, NULL);

    op = cmd_code(JBOD_WRITE_BLOCK, 0, 0, 0);
    jbod_client_operation(op, temp);

    // Update number of bytes written and current address
    totalBytesWritten += bytesToWrite;
    addr += bytesToWrite;
  }

  return len;
}
