// Main file for transport protocol app

#include "options.h"
#include "defines.h"
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

void test_send_bulk() {
  const char data[] = "Test123";
  size_t size = sizeof(data);
  int64_t res = send_bulk(TEST_OUT_ADDR, PORT_SERVER, data, size);
  if (res != ERR_SUCCESS) {
    print_send_error(res);
  }
  puts("Sent data successfully");
}

void test_send_stream(const char* fname) {
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
  int64_t res = send_stream(TEST_OUT_ADDR, PORT_SERVER, fbuf, fsize);
  if (res != ERR_SUCCESS) {
    print_send_error(res);
  }
  free(fbuf);
  puts("Sent data successfully");
}

int main(int argc, char* argv[]) {
  create_socket();
  if (argc == 1) {
    bind_to_port(PORT_SERVER);
    while (1) {
      listen_for_data();
    }
  } else if (argc == 2) {
    if (!strcmp(argv[1], "--test-bulk")) {
      bind_to_port(PORT_CLIENT);
      test_send_bulk();
    } else {
      die("cannot run with given arguments");
    }
  } else if (argc == 3) {
    if (!strcmp(argv[1], "--file")) {
      bind_to_port(PORT_CLIENT);
      test_send_stream(argv[2]);
    } else {
      die("cannot run with given arguments");
    }
  } else {
    die("cannot run with given arguments");
  }
  close_socket();
  return 0;
}
