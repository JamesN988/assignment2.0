#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "gbn.h"

#define RTT  16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

struct sender_packet {
    struct pkt packet;
    bool acked;
    bool sent;
  };
  
static struct sender_packet A_buffer[SEQSPACE];
static int A_base = 0;
static int A_nextseq = 0;

int ComputeChecksum(struct pkt packet) {
  int checksum = packet.seqnum + packet.acknum;
  int i;
  for (i = 0; i < 20; i++)
      checksum += packet.payload[i];
  return checksum;
}

bool IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}


void A_output(struct msg message) {
    struct pkt pkt;
    int i;

    if (((A_nextseq - A_base + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {

        if (TRACE > 1)
            printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

        pkt.seqnum = A_nextseq;
        pkt.acknum = NOTINUSE;

        for (i = 0; i < 20; i++)
            pkt.payload[i] = message.data[i];

        pkt.checksum = ComputeChecksum(pkt);
        A_buffer[A_nextseq].packet = pkt;
        A_buffer[A_nextseq].acked = false;
        A_buffer[A_nextseq].sent = true;

        if (TRACE > 0)
            printf("Sending packet %d to layer 3\n", pkt.seqnum);
        
        tolayer3(A, pkt);

        if (A_base == A_nextseq) {
            starttimer(A, RTT); 
        }

        A_nextseq = (A_nextseq + 1) % SEQSPACE;
    } else {
        if (TRACE > 0)
            printf("----A: New message arrives, send window is full\n");
        window_full++;
    }
}


void A_input(struct pkt packet) {
    int i;

    if (!IsCorrupted(packet)) {
        if (TRACE > 0)
            printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
        total_ACKs_received++;

        if (A_buffer[packet.acknum].sent && !A_buffer[packet.acknum].acked) {
            if (TRACE > 0)
                printf("----A: ACK %d is not a duplicate\n", packet.acknum);
            A_buffer[packet.acknum].acked = true;
            new_ACKs++;
        } else {
            if (TRACE > 0)
                printf("----A: duplicate ACK received, do nothing!\n");
        }

        while (A_buffer[A_base].acked) {
            A_buffer[A_base].sent = false;
            A_base = (A_base + 1) % SEQSPACE;
        }

        stoptimer(A);
        for (i = 0; i < WINDOWSIZE; i++) {
            int idx = (A_base + i) % SEQSPACE;
            if (A_buffer[idx].sent && !A_buffer[idx].acked) {
                starttimer(A, RTT);
                break;
            }
        }
    } else {
        if (TRACE > 0)
            printf("----A: corrupted ACK is received, do nothing!\n");
    }
}
void A_timerinterrupt(void) {
    int i;
    if (TRACE > 0)
        printf("----A: time out,resend packets!\n");

    for (i = 0; i < WINDOWSIZE; i++) {
        int idx = (A_base + i) % SEQSPACE;
        if (A_buffer[idx].sent && !A_buffer[idx].acked) {
            if (TRACE > 0)
                printf("---A: resending packet %d\n", A_buffer[idx].packet.seqnum);
            tolayer3(A, A_buffer[idx].packet);
            packets_resent++;
            if (i == 0)
                starttimer(A, RTT);
        }
    }
}

void A_init(void) {
  int i;  
  for (i = 0; i < SEQSPACE; i++)
      A_buffer[i].acked = A_buffer[i].sent = false;
  A_base = 0;
  A_nextseq = 0;
}

/* Receiver B */

static struct pkt B_buffer[SEQSPACE];
static bool B_received[SEQSPACE];
static int B_expected = 0;

void B_input(struct pkt packet) {
  struct pkt ackpkt;
  int i;
  int seq;

  if (!IsCorrupted(packet)) {
    seq = packet.seqnum;

    if (((seq - B_expected + SEQSPACE) % SEQSPACE) < WINDOWSIZE) {
        if (!B_received[seq]) {
            B_buffer[seq] = packet;
            B_received[seq] = true;
            packets_received++;
        }

        while (B_received[B_expected]) {
            tolayer5(B, B_buffer[B_expected].payload);
            B_received[B_expected] = false;
            B_expected = (B_expected + 1) % SEQSPACE;
        }

        if (TRACE > 0)
            printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
    } else {
        if (TRACE > 0)
            printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
    }

    ackpkt.seqnum = 0;
    ackpkt.acknum = seq;
    for (i = 0; i < 20; i++)
        ackpkt.payload[i] = 0;
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);
} else {
    if (TRACE > 0)
        printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
    seq = (B_expected == 0) ? SEQSPACE - 1 : B_expected - 1;

    ackpkt.seqnum = 0;
    ackpkt.acknum = seq;
    for (i = 0; i < 20; i++)
        ackpkt.payload[i] = 0;
    ackpkt.checksum = ComputeChecksum(ackpkt);
    tolayer3(B, ackpkt);
}
}

void B_init(void)
{
    int i;
    for (i=0; i < SEQSPACE; i++)
        B_received[i] = false;
    B_expected = 0;
}

void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
