// This file includes defines

#pragma once

#include "options.h"
#include <inttypes.h>

// Flags
#define FLAG_SYN 1
#define FLAG_ACK 2
#define FLAG_FIN 4
#define FLAG_RST 8
#define FLAG_DAT 16
#define FLAG_BLK 32
#define FLAG_RESERVED_7 64
#define FLAG_RESERVED_8 128

// Field positions
#define POS_CHKSUM 0
#define POS_VER 4
#define POS_FLAGS 5
#define POS_DYN_DATA 6

// Error codes
#define ERR_SUCCESS 0
#define ERR_VER_INCOMPAT -1
#define ERR_PACKET_SHORT -2
#define ERR_HASH_WRONG -3
#define ERR_BLK_TOO_MUCH_DATA -4
#define ERR_CANNOT_SEND -5
#define ERR_CANNOT_RECEIVE -6
#define ERR_WRONG_FLAG -7
#define ERR_ACKNUM_DECR -8
#define ERR_NO_ACK -9
#define ERR_WRONG_ACKNUM -10
#define ERR_TOO_MANY_RETRANSMISSIONS -11
#define ERR_INET_ATON -12
#define ERR_UNKNOWN INT64_MIN

// Number handling
#define SEQNUM_MAX UINT64_MAX
#define BULK_PAYLOAD_MAX BUFLEN - POS_DYN_DATA
#define SEQNUM_PRINTF_CONST PRIu32
#define RETCODE_PRINTF_CONST PRId64
