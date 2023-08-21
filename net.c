#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf)
{
  int bytes_read = 0;
  int curr_bytes_read;

  while (bytes_read < len)
  {
    curr_bytes_read = read(fd, buf + bytes_read, len - bytes_read);

    if (curr_bytes_read == -1)
    {
      return false;
    }
    bytes_read += curr_bytes_read;
  }

  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf)
{
  int bytes_written = 0;
  int curr_bytes_written;

  while (bytes_written < len)
  {
    curr_bytes_written = write(fd, buf + bytes_written, len - bytes_written);

    if (curr_bytes_written == -1)
    {
      return false;
    }
    bytes_written += curr_bytes_written;
  }

  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block)
{
  uint8_t packet_header[HEADER_LEN];
  uint16_t length;

  // Read the packet header and return false if unsuccessful
  if (!nread(fd, HEADER_LEN, packet_header))
  {
    return false;
  }

  // Extract the length, op, and ret, and convert them to host byte order
  memcpy(&length, packet_header, 2);
  length = ntohs(length);
  memcpy(op, packet_header + 2, 4);
  *op = ntohl(*op);
  memcpy(ret, packet_header + 6, 2);
  *ret = ntohs(*ret);

  // If there is a block of data to read, read it and return the success status
  return (length != (HEADER_LEN + JBOD_BLOCK_SIZE)) || nread(fd, JBOD_BLOCK_SIZE, block);
}

/* Helper function to make a packet with the given length, operation code, return code, and data block. */
uint8_t *create_network_packet(uint16_t length, uint32_t op, uint16_t return_code, uint8_t *data_block)
{
  uint8_t *packet = (uint8_t *)malloc(length);
  int packet_length = length;

  // Convert the length, op, and return_code to network byte order
  length = htons(length);
  op = htonl(op);
  return_code = htons(return_code);

  // Write the length, op, and return_code to the packet
  memcpy(packet, &length, 2);
  memcpy(packet + 2, &op, 4);
  memcpy(packet + 6, &return_code, 2);

  // If the packet includes a data_block, copy it
  if (packet_length == (8 + JBOD_BLOCK_SIZE))
  {
    memcpy(packet + 8, data_block, JBOD_BLOCK_SIZE);
  }

  return packet;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint32_t length = HEADER_LEN + (((op >> 26) == JBOD_WRITE_BLOCK) ? 256 : 0);
  uint16_t return_code = 0;

  uint8_t *packet = create_network_packet(length, op, return_code, block);

  // Send the packet and free the allocated memory
  bool success = nwrite(sd, length, packet);
  free(packet);

  return success;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port)
{
  struct sockaddr_in server_address;

  // Configure the server address
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);

  if (inet_aton(ip, &server_address.sin_addr) == 0)
  {
    return false;
  }

  // Create a socket and connect to the server
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1 ||
      connect(cli_sd, (const struct sockaddr *)&server_address, sizeof(server_address)) == -1)
  {
    return false;
  }

  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void)
{
  close(cli_sd);
  cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block)
{
  uint16_t return_value;

  if (send_packet(cli_sd, op, block) && recv_packet(cli_sd, &op, &return_value, block))
  {
    return return_value;
  }

  return -1;
}
