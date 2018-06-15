// External API of transport protocol

#pragma once

#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// Struct for holding disassembled message
struct tproto_message {
  uint32_t seq_num;
  uint32_t ack_num;
  uint8_t flags;
  char data[BUFLEN];
};

// Die utility function in case of errors
void die(const char* restrict s);

// Receives one packet from specified host
ssize_t receive_packet_from_other(
  char* restrict buf,
  const struct sockaddr_in* restrict other
);

// Assembles a message
int64_t assemble_message(
  char* restrict buf,
  uint8_t flags,
  uint32_t seq_num,
  uint32_t ack_num,
  const char* restrict data,
  size_t* restrict data_len
);

// Disassembles a message
int64_t disassemble_message(
  const char* restrict buf,
  ssize_t recv_len,
  struct tproto_message* restrict mes
);

// Create socket and store in variable `sock`
void create_socket();

// Close socket in variable `sock`
void close_socket();

// Binds `sock` to given port
void bind_to_port(uint16_t port);

// Listens for data to receive
void listen_for_data();

// Sends data in bulk packet
int64_t send_bulk(
  const char* restrict to_addr,
  uint16_t to_port,
  const char* restrict data,
  size_t bytes
);

// Sends data as packet stream
int64_t send_stream(
  const char* restrict to_addr,
  uint16_t to_port,
  const char* restrict data,
  size_t bytes
);
