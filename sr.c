#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

int ComputeChecksum(struct pkt packet) {
  int checksum = packet.seqnum + packet.acknum;
  for (int i = 0; i < 20; i++)
      checksum += packet.payload[i];
  return checksum;
}

bool IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}

struct sender_packet {
  struct pkt packet;
  bool acked;
  bool sent;
};

static struct sender_packet A_buffer[SEQSPACE];
static int A_base = 0;
static int A_nextseq = 0;
