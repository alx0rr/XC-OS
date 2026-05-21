#ifndef NE2000_H
#define NE2000_H

#include "../../lib/types.h"

#define NE_BASE       0x300
#define NE_IRQ        9

#define NE_CMD        0x00
#define NE_PSTART     0x01
#define NE_PSTOP      0x02
#define NE_BNRY       0x03
#define NE_TPSR       0x04
#define NE_TBCR0      0x05
#define NE_TBCR1      0x06
#define NE_ISR        0x07
#define NE_RSAR0      0x08
#define NE_RSAR1      0x09
#define NE_RBCR0      0x0A
#define NE_RBCR1      0x0B
#define NE_RCR        0x0C
#define NE_TCR        0x0D
#define NE_DCR        0x0E
#define NE_IMR        0x0F
#define NE_DATA       0x10
#define NE_RESET      0x1F

#define NE_P1_PAR0    0x01
#define NE_P1_CURR    0x07
#define NE_P1_MAR0    0x08

#define NE_CMD_STOP   0x01
#define NE_CMD_START  0x02
#define NE_CMD_TX     0x04
#define NE_CMD_RD0    0x08
#define NE_CMD_RD1    0x10
#define NE_CMD_RD2    0x20
#define NE_CMD_PS0    0x00
#define NE_CMD_PS1    0x40
#define NE_CMD_PS2    0x80

#define NE_ISR_PRX    0x01
#define NE_ISR_PTX    0x02
#define NE_ISR_RXE    0x04
#define NE_ISR_TXE    0x08
#define NE_ISR_OVW    0x10
#define NE_ISR_RST    0x80

#define NE_RCR_AB     0x04
#define NE_RCR_AM     0x08
#define NE_RCR_PRO    0x10

#define NE_DCR_WTS    0x01
#define NE_DCR_BOS    0x02
#define NE_DCR_LAS    0x04
#define NE_DCR_LS     0x08
#define NE_DCR_FT1    0x40

#define NE_TX_PAGE    0x40
#define NE_RX_START   0x46
#define NE_RX_STOP    0x80

#define ETH_ALEN      6
#define ETH_MTU       1500
#define ETH_FRAME_MAX 1518

typedef struct {
    u8  status;
    u8  next;
    u16 len;
} PACKED ne_rx_hdr_t;

int  ne2000_init(void);
int  ne2000_send(const u8 *buf, u16 len);
void ne2000_poll(void);
void ne2000_get_mac(u8 *mac);
int  ne2000_present(void);

#endif
