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

void A_output(struct msg message) {
  if (((A_nextseq - A_base + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {
      struct pkt pkt;
      pkt.seqnum = A_nextseq;
      pkt.acknum = NOTINUSE;

      for (int i = 0; i < 20; i++)
          pkt.payload[i] = message.data[i];

      pkt.checksum = ComputeChecksum(pkt);
      A_buffer[A_nextseq].packet = pkt;
      A_buffer[A_nextseq].acked = false;
      A_buffer[A_nextseq].sent = true;

      tolayer3(A, pkt);
      starttimer(A, RTT);

      if (TRACE > 1)
          printf("A_output: Sent packet %d\n", pkt.seqnum);

      A_nextseq = (A_nextseq + 1) % SEQSPACE;
  } else {
      if (TRACE > 0)
          printf("A_output: Window full, message dropped\n");
      window_full++;
  }
}
