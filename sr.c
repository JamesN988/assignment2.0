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

void A_input(struct pkt packet) {
  if (!IsCorrupted(packet)) {
      int acknum = packet.acknum;
      if (TRACE > 1)
          printf("A_input: ACK %d received\n", acknum);

      if (A_buffer[acknum].sent && !A_buffer[acknum].acked) {
          A_buffer[acknum].acked = true;
          new_ACKs++;
          total_ACKs_received++;
      }

      // Slide window if base is ACKed
      while (A_buffer[A_base].acked) {
          A_buffer[A_base].sent = false;
          A_base = (A_base + 1) % SEQSPACE;
      }

      stoptimer(A);
      for (int i = 0; i < WINDOWSIZE; i++) {
          int idx = (A_base + i) % SEQSPACE;
          if (A_buffer[idx].sent && !A_buffer[idx].acked) {
              starttimer(A, RTT);
              break;
          }
      }
  } else {
      if (TRACE > 0)
          printf("A_input: Corrupted ACK received, ignored\n");
  }
}
void A_timerinterrupt(void) {
  if (TRACE > 0)
      printf("A_timerinterrupt: Retransmitting unacked packets\n");

  for (int i = 0; i < WINDOWSIZE; i++) {
      int idx = (A_base + i) % SEQSPACE;
      if (A_buffer[idx].sent && !A_buffer[idx].acked) {
          tolayer3(A, A_buffer[idx].packet);
          packets_resent++;
          if (i == 0)
              starttimer(A, RTT);
      }
  }
}
