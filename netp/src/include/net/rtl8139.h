#ifndef RTL8139_H
#define RTL8139_H

#include "../../lib/types.h"

#define RTL8139_IDR0        0x00
#define RTL8139_MAR0        0x08
#define RTL8139_TXSTATUS0   0x10
#define RTL8139_TXADDR0     0x20
#define RTL8139_RXBUF       0x30
#define RTL8139_RXEARLYCNT  0x34
#define RTL8139_RXEARLYSTATUS 0x36
#define RTL8139_CHIPCMD     0x37
#define RTL8139_RXBUFTAIL   0x38
#define RTL8139_RXBUFHEAD   0x3A
#define RTL8139_INTRMASK    0x3C
#define RTL8139_INTRSTATUS  0x3E
#define RTL8139_TXCONFIG    0x40
#define RTL8139_RXCONFIG    0x44
#define RTL8139_TIMER       0x48
#define RTL8139_RXMISSED    0x4C
#define RTL8139_CFG9346     0x50
#define RTL8139_CONFIG0     0x51
#define RTL8139_CONFIG1     0x52
#define RTL8139_TIMERINT    0x54
#define RTL8139_MSR         0x58
#define RTL8139_CONFIG3     0x59
#define RTL8139_CONFIG4     0x5A
#define RTL8139_MPC         0x5C
#define RTL8139_MULINT      0x5C
#define RTL8139_RERID       0x5E
#define RTL8139_TSAD        0x60
#define RTL8139_BMCR        0x62
#define RTL8139_BMSR        0x64
#define RTL8139_ANAR        0x66
#define RTL8139_ANLPAR      0x68
#define RTL8139_ANER        0x6A
#define RTL8139_DIS         0x6C
#define RTL8139_FCSC        0x6E
#define RTL8139_NWAYTR      0x70
#define RTL8139_REC         0x72
#define RTL8139_CSCR        0x74
#define RTL8139_PHY1_PARM   0x78
#define RTL8139_TW_PARM     0x7C
#define RTL8139_PHY2_PARM   0x80

#define RTL8139_CMD_RESET   0x10
#define RTL8139_CMD_RX_ENABLE 0x08
#define RTL8139_CMD_TX_ENABLE 0x04
#define RTL8139_CMD_BUF_EMPTY 0x01

#define RTL8139_INT_RXOK    0x0001
#define RTL8139_INT_RXERR   0x0002
#define RTL8139_INT_TXOK    0x0004
#define RTL8139_INT_TXERR   0x0008
#define RTL8139_INT_RXOVW   0x0010
#define RTL8139_INT_LINKCHG 0x0020
#define RTL8139_INT_RXFIFOOVW 0x0040
#define RTL8139_INT_TIMEOUT 0x4000
#define RTL8139_INT_SERR    0x8000

#define RTL8139_CFG_EEM0    0x40
#define RTL8139_CFG_EEM1    0x80

#define RTL8139_RX_AAP      0x00000001
#define RTL8139_RX_APM      0x00000002
#define RTL8139_RX_AM       0x00000004
#define RTL8139_RX_AB       0x00000008
#define RTL8139_RX_AR       0x00000010
#define RTL8139_RX_AER      0x00000020
#define RTL8139_RX_WRAP     0x00000080
#define RTL8139_RX_MXDMA_SHIFT 8
#define RTL8139_RX_RBLEN_SHIFT 11
#define RTL8139_RX_FTH_SHIFT 13

#define RTL8139_TX_MXDMA_SHIFT 8
#define RTL8139_TX_RETRYCNT_SHIFT 4

#define RTL8139_TX_HOST_OWNS 0x00002000
#define RTL8139_TX_UNDERRUN  0x00004000
#define RTL8139_TX_STAT_OK   0x00008000
#define RTL8139_TX_OUT_OF_WINDOW 0x20000000
#define RTL8139_TX_ABORTED   0x40000000
#define RTL8139_TX_CARRIER_LOST 0x80000000

#define RTL8139_RX_BUF_SIZE 8192
#define RTL8139_TX_BUF_SIZE 2048
#define RTL8139_NUM_TX_DESC 4

typedef struct {
    uint16_t io_base;
    uint8_t irq;
    uint8_t mac_addr[6];
    uint8_t* rx_buffer;
    uint8_t* tx_buffer[RTL8139_NUM_TX_DESC];
    uint32_t rx_buffer_phys;
    uint32_t tx_buffer_phys[RTL8139_NUM_TX_DESC];
    uint16_t rx_offset;
    uint8_t tx_current;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_errors;
    uint32_t tx_errors;
    uint8_t link_up;
} rtl8139_device_t;

void rtl8139_init(void);
int rtl8139_send_packet(const void* data, uint32_t len);
void rtl8139_receive_packet(void);
void rtl8139_irq_handler(void);
void rtl8139_get_mac_address(uint8_t* mac);
uint8_t rtl8139_is_link_up(void);
void rtl8139_print_stats(void);

#endif
