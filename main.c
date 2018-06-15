// Main file for transport protocol app

#include "options.h"
#include "defines.h"
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

void print_send_error(int64_t res) {
  char* errmes;
  int printres = asprintf(
    &errmes,
    "failed to send, received error %+" RETCODE_PRINTF_CONST,
    res
  );
  if (printres < 0) {
    die("failed to send, could not print error");
  }
  die(errmes);
}

void test_send_bulk(
  const char* restrict data,
  const char* restrict addr,
  uint16_t port
) {
  int64_t res = send_bulk(addr, port, data, strlen(data));
  if (res != ERR_SUCCESS) {
    print_send_error(res);
  }
  puts("Sent data successfully");
}

void test_send_stream(
  const char* restrict fname,
  const char* restrict addr,
  uint16_t port
) {
  FILE* fd = fopen(fname, "r");
  if (fd == NULL) {
    die("could not open file");
  }
  if (fseek(fd, 0, SEEK_END) < 0) {
    die("failed to seek to end");
  }
  off_t _fsize = ftello(fd);
  if (_fsize < 0) {
    die("failed to get value of file position indicator");
  }
  size_t fsize = _fsize;
  if (fsize > SEQNUM_MAX) {
    die("file is larger than highest sequence number");
  }
  char* fbuf = malloc(fsize);
  if (fbuf == NULL) {
    die("could not allocate memory for file");
  }
  rewind(fd);
  if (fread(fbuf, fsize, 1, fd) < 1) {
    die("failed to read in file");
  }
  if (fclose(fd) == EOF) {
    die("failed to close file");
  }
  int64_t res = send_stream(addr, port, fbuf, fsize);
  if (res != ERR_SUCCESS) {
    print_send_error(res);
  }
  free(fbuf);
  puts("Sent data successfully");
}

uint16_t readPort(char* restrict in) {
  char* end;
  long port = strtol(in, &end, 10);
  if (errno != 0 || port < 0 || port > UINT16_MAX) {
    die("failed to read port number from argument");
  }
  return port;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    die("not enough arguments");
  }
  create_socket();
  uint16_t own_port = readPort(argv[1]);
  bind_to_port(own_port);
  if (argc == 2) {
    while (1) {
      listen_for_data();
    }
  } else if (argc == 6) {
    uint16_t dest_port = readPort(argv[3]);
    if (!strcmp(argv[4], "--bulk")) {
      test_send_bulk(argv[5], argv[2], dest_port);
    } else if (!strcmp(argv[4], "--file")) {
      test_send_stream(argv[5], argv[2], dest_port);
    } else {
      die("cannot run with given arguments");
    }
  } else {
    die("cannot run with given arguments");
  }
  close_socket();
  return 0;
}
