// Transport protocol implementation file

#include "options.h"
#include "defines.h"
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <mhash.h>

// Socket number (not thread-safe)
int sock;

void die(const char* restrict s) {
  perror(s);
  exit(EXIT_FAILURE);
}

uint32_t generate_hash(const char* restrict input, size_t len) {
  MHASH mhash_thread = mhash_init(MHASH_CRC32);
  if (mhash_thread == MHASH_FAILED) {
    die("could not initialize mhash");
  }
  mhash(mhash_thread, input, len);
  uint32_t hash;
  mhash_deinit(mhash_thread, &hash);
  return hash;
}

int check_hash(uint32_t hash1, uint32_t hash2) {
  return hash1 == hash2;
}

int same_host(
  const struct sockaddr_in* restrict one,
  const struct sockaddr_in* restrict two
) {
  return one->sin_family == two->sin_family &&
    one->sin_port == two->sin_port &&
    one->sin_addr.s_addr == two->sin_addr.s_addr;
}

ssize_t receive_packet_from_other(
  char* restrict buf,
  const struct sockaddr_in* restrict other
) {
  struct sockaddr_in recv_addr;
  socklen_t recv_socklen = sizeof(recv_addr);
  ssize_t recv_len;
  do {
    recv_len = recvfrom(
      sock,
      buf,
      BUFLEN,
      0,
      (struct sockaddr*)&recv_addr,
      (socklen_t*)&recv_socklen
    );
    if (recv_len < 0) {
      return ERR_CANNOT_RECEIVE;
    }
  } while (
    // Skip packets from other hosts
    !same_host(&recv_addr, other)
  );
  return recv_len;
}

int64_t assemble_message(
  char* restrict buf,
  uint8_t flags,
  uint32_t seq_num,
  uint32_t ack_num,
  const char* restrict data,
  size_t* restrict data_len
) {
  buf[POS_VER] = PROTO_VER;
  buf[POS_FLAGS] = flags;
  size_t pos = POS_DYN_DATA;
  if ((flags & FLAG_DAT) && !(flags & FLAG_BLK)) {
    seq_num = htonl(seq_num);
    memcpy(buf + pos, &seq_num, sizeof(seq_num));
    pos += sizeof(seq_num);
  }
  if (flags & FLAG_ACK) {
    ack_num = htonl(ack_num);
    memcpy(buf + pos, &ack_num, sizeof(ack_num));
    pos += sizeof(ack_num);
  }
  if ((flags & FLAG_DAT) || (flags & FLAG_BLK)) {
    if (*data_len > BUFLEN - pos) {
      if (flags & FLAG_BLK) {
        return ERR_BLK_TOO_MUCH_DATA;
      }
      *data_len = BUFLEN - pos;
    }
    memcpy(buf + pos, data, *data_len);
    pos += *data_len;
  }
  uint32_t hash = htonl(generate_hash(buf + POS_VER, pos - POS_VER));
  memcpy(buf + POS_CHKSUM, &hash, sizeof(hash));
  return pos;
}

void add_fin_flag(char* restrict buf, size_t len) {
  buf[POS_FLAGS] |= FLAG_FIN;
  uint32_t hash = htonl(generate_hash(buf + POS_VER, len - POS_VER));
  memcpy(buf + POS_CHKSUM, &hash, sizeof(hash));
}

int64_t disassemble_message(
  const char* restrict buf,
  ssize_t recv_len,
  struct tproto_message* restrict mes
) {
  if (recv_len < POS_DYN_DATA) {
    return ERR_PACKET_SHORT;
  }
  uint32_t hash;
  memcpy(&hash, buf + POS_CHKSUM, sizeof(hash));
  hash = ntohl(hash);
  if (!check_hash(hash, generate_hash(buf + POS_VER, recv_len - POS_VER))) {
    return ERR_HASH_WRONG;
  }
  if (buf[POS_VER] != 1) {
    return ERR_VER_INCOMPAT;
  }
  mes->flags = buf[POS_FLAGS];
  size_t pos = POS_DYN_DATA;
  if ((mes->flags & FLAG_DAT) && !(mes->flags & FLAG_BLK)) {
    if (recv_len < pos + sizeof(mes->seq_num)) {
      return ERR_PACKET_SHORT;
    }
    memcpy(&mes->seq_num, buf + pos, sizeof(mes->seq_num));
    mes->seq_num = ntohl(mes->seq_num);
    pos += sizeof(mes->seq_num);
  }
  if (mes->flags & FLAG_ACK) {
    if (recv_len < pos + sizeof(mes->ack_num)) {
      return ERR_PACKET_SHORT;
    }
    memcpy(&mes->ack_num, buf + pos, sizeof(mes->ack_num));
    mes->ack_num = ntohl(mes->ack_num);
    pos += sizeof(mes->ack_num);
  }
  if ((mes->flags & FLAG_DAT) || (mes->flags & FLAG_BLK)) {
    size_t len = recv_len - pos;
    memcpy(mes->data, buf + pos, len);
    return len;
  }
  return ERR_SUCCESS;
}

void create_socket() {
  // Create a UDP socket
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    die("failed to create socket");
  }
}

void close_socket() {
  if (close(sock) < 0) {
    die("could not close socket");
  }
}

void bind_to_port(uint16_t port) {
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  // Bind socket to port
  if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    die("failed to bind to port");
  }
}

void listen_for_data() {
  struct sockaddr_in* other_addr = NULL;
  socklen_t other_socklen = sizeof(*other_addr);
  struct sockaddr_in temp_addr;
  socklen_t socklen = sizeof(temp_addr);
  size_t file_buf_size = 1024;
  char* file_buf = malloc(file_buf_size);
  if (!file_buf) {
    die("failed to malloc");
  }
  size_t amount_recvd = 0;
  char buf[BUFLEN];
  int syn = 0;
  int running = 1;
  puts("Waiting for connection...");
  do {
    ssize_t recv_len = recvfrom(
      sock,
      buf,
      BUFLEN,
      0,
      (struct sockaddr*)&temp_addr,
      (socklen_t*)&socklen
    );
    if (recv_len < 0) {
      fputs("failed to receive data\n", stderr);
      free(file_buf);
      free(other_addr);
      return;
    }
    if (!other_addr) {
      other_addr = malloc(sizeof(*other_addr));
      if (!other_addr) {
        die("failed to malloc");
      }
      *other_addr = temp_addr;
      other_socklen = socklen;
      puts("Working on connection...");
    } else if (!same_host(&temp_addr, other_addr)) {
      // Received packet from another address, skip
      continue;
    }
    struct tproto_message mes;
    int64_t len = disassemble_message(buf, recv_len, &mes);
    if (len == ERR_HASH_WRONG || mes.seq_num != amount_recvd) {
      // Handle faulty packet
      int64_t res = assemble_message(buf, FLAG_ACK, 0, amount_recvd, NULL, 0);
      if (res < 0) {
        fputs("could not assemble message\n", stderr);
        free(file_buf);
        free(other_addr);
        return;
      }
      ssize_t send_res = sendto(
        sock,
        buf,
        res,
        0,
        (struct sockaddr*)other_addr,
        other_socklen
      );
      continue;
    } else if (len < 0) {
      fputs("could not disassemble message\n", stderr);
      free(file_buf);
      free(other_addr);
      return;
    }
    uint8_t send_flags = FLAG_ACK;
    if (!syn) {
      if (mes.flags & FLAG_BLK) {
        running = 0;
      } else if (mes.flags & FLAG_SYN) {
        syn = 1;
      } else {
        fputs("first recvd packet was not a syn packet\n", stderr);
        free(file_buf);
        free(other_addr);
        return;
      }
    }
    if (file_buf_size - amount_recvd < len) {
      file_buf_size *= 2;
      file_buf = realloc(file_buf, file_buf_size);
      if (!file_buf) {
        fputs("failed to reallocate buffer\n", stderr);
        free(file_buf);
        free(other_addr);
        return;
      }
    }
    memcpy(file_buf + amount_recvd, mes.data, len);
    amount_recvd += len;
    if (mes.flags & FLAG_FIN) {
      running = 0;
    }
    int64_t res = assemble_message(buf, send_flags, 0, amount_recvd, NULL, 0);
    if (res < 0) {
      fputs("could not assemble message\n", stderr);
      free(file_buf);
      free(other_addr);
      return;
    }
    ssize_t send_res = sendto(
      sock,
      buf,
      res,
      0,
      (struct sockaddr*)other_addr,
      other_socklen
    );
    if (send_res < 0) {
      fputs("failed to send\n", stderr);
      free(file_buf);
      free(other_addr);
      return;
    }
  } while (running);
  const char file_name_template[] = "received/recvd.XXXXXX";
  char* file_name = malloc(sizeof(file_name_template));
  if (!file_name) {
    die("failed to malloc");
  }
  strcpy(file_name, file_name_template);
  int out_file = mkstemp(file_name);
  if (!out_file) {
    die("could not create output file");
  }
  if (write(out_file, file_buf, amount_recvd) < 0) {
    die("could not write to file");
  }
  if (close(out_file) < 0) {
    die("could not close file");
  };
  printf(
    "Received data:\n"
    "amount: %zu bytes\n"
    "file name: %s\n"
    "content: %s\n",
    amount_recvd,
    file_name,
    file_buf
  );
  free(file_buf);
  free(other_addr);
  free(file_name);
}

int64_t send_bulk(
  const char* restrict to_addr,
  uint16_t to_port,
  const char* restrict data,
  size_t bytes
) {
    struct sockaddr_in other_addr;
    other_addr.sin_family = AF_INET;
    other_addr.sin_port = htons(to_port);
    if (!inet_aton(to_addr, (struct in_addr*)&other_addr.sin_addr.s_addr)) {
      return ERR_INET_ATON;
    }
    socklen_t socklen = sizeof(other_addr);
    char buf[BUFLEN];
    int64_t len = assemble_message(buf, FLAG_BLK, 0, 0, data, &bytes);
    if (len < 0) {
      return len;
    }
    ssize_t res = sendto(
      sock,
      buf,
      len,
      0,
      (struct sockaddr*)&other_addr,
      socklen
    );
    if (res < 0) {
      return ERR_CANNOT_SEND;
    }
    len = receive_packet_from_other(buf, &other_addr);
    if (len < 0) {
      return len;
    }
    struct tproto_message mes;
    len = disassemble_message(buf, len, &mes);
    if (len < 0) {
      return len;
    }
    if (!(mes.flags & FLAG_ACK) || mes.ack_num != bytes) {
      return ERR_NO_ACK;
    }
    return ERR_SUCCESS;
}

int64_t send_stream(
  const char* restrict to_addr,
  uint16_t to_port,
  const char* restrict data,
  size_t bytes
) {
  struct sockaddr_in other_addr;
  other_addr.sin_family = AF_INET;
  other_addr.sin_port = htons(to_port);
  if (!inet_aton(to_addr, (struct in_addr*)&other_addr.sin_addr.s_addr)) {
    return ERR_INET_ATON;
  }
  socklen_t socklen = sizeof(other_addr);
  char buf[BUFLEN];
  size_t data_len = 0;
  int64_t len = assemble_message(buf, FLAG_SYN, 0, 0, NULL, &data_len);
  if (len < 0) {
    die("failed to assemble message");
  }
  ssize_t res = sendto(
    sock,
    buf,
    len,
    0,
    (struct sockaddr*)&other_addr,
    socklen
  );
  if (res < 0) {
    die("failed to send");
  }
  len = receive_packet_from_other(buf, &other_addr);
  if (len < 0) {
    return len;
  }
  struct tproto_message mes;
  len = disassemble_message(buf, len, &mes);
  if (len < 0) {
    return len;
  } else if (!(mes.flags & FLAG_ACK)) {
    return ERR_NO_ACK;
  } else if (mes.ack_num != 0) {
    return ERR_WRONG_ACKNUM;
  }
  uint32_t ack_num = 0;
  unsigned char retransmit_count = 0;
  while (1) {
    size_t data_len = bytes - ack_num;
    len = assemble_message(
      buf,
      FLAG_DAT,
      ack_num,
      0,
      data + ack_num,
      &data_len
    );
    if (len < 0) {
      return len;
    }
    if (bytes - data_len - ack_num == 0) {
      add_fin_flag(buf, len);
    }
    res = sendto(
      sock,
      buf,
      len,
      0,
      (struct sockaddr*)&other_addr,
      socklen
    );
    if (res < 0) {
      return ERR_CANNOT_SEND;
    }
    len = receive_packet_from_other(buf, &other_addr);
    if (len < 0) {
      return len;
    }
    len = disassemble_message(buf, len, &mes);
    if (len < 0) {
      return len;
    }
    if (!(mes.flags & FLAG_ACK)) {
      return ERR_WRONG_FLAG;
    }
    if (mes.ack_num < ack_num) {
      return ERR_ACKNUM_DECR;
    }
    if (ack_num == mes.ack_num) {
      if (retransmit_count == 10) {
        return ERR_TOO_MANY_RETRANSMISSIONS;
      }
      ++retransmit_count;
      continue;
    }
    if (mes.ack_num == bytes) {
      break;
    }
    retransmit_count = 0;
    ack_num = mes.ack_num;
  }
  return ERR_SUCCESS;
}
