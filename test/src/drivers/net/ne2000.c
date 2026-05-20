#include "../../include/net/ne2000.h"
#include "../../include/net/eth.h"
#include "../../include/interrupts/idt.h"
#include "../../include/timer/pit.h"
#include "../../lib/io.h"
#include "../../lib/string.h"
#include "../../lib/types.h"

static u8  mac[ETH_ALEN];
static u8  rx_next;
static int ready = 0;

static inline u8  rd(u8 r)         { return inb(NE_BASE + r); }
static inline void wr(u8 r, u8 v)  { outb(NE_BASE + r, v); }

static void pg(u8 p) {
    u8 c = rd(NE_CMD) & ~(NE_CMD_PS1 | NE_CMD_PS0);
    wr(NE_CMD, c | (p << 6));
}

static void dma_read(u16 src, void *dst, u16 len) {
    u8 *p = (u8*)dst;
    u16 i;
    wr(NE_CMD,  NE_CMD_RD2 | NE_CMD_PS0 | NE_CMD_START);
    wr(NE_RBCR0, len & 0xFF);
    wr(NE_RBCR1, len >> 8);
    wr(NE_RSAR0, src & 0xFF);
    wr(NE_RSAR1, src >> 8);
    wr(NE_CMD,  NE_CMD_RD0 | NE_CMD_START);
    for (i = 0; i < len; i++)
        p[i] = inb(NE_BASE + NE_DATA);
    wr(NE_CMD,  NE_CMD_RD2 | NE_CMD_START);
}

static void dma_write(u16 dst, const void *src, u16 len) {
    const u8 *p = (const u8*)src;
    u16 i;
    u16 sz = (len < 64) ? 64 : len;
    wr(NE_CMD,  NE_CMD_RD2 | NE_CMD_START);
    wr(NE_RBCR0, sz & 0xFF);
    wr(NE_RBCR1, sz >> 8);
    wr(NE_RSAR0, dst & 0xFF);
    wr(NE_RSAR1, dst >> 8);
    wr(NE_CMD,  NE_CMD_RD1 | NE_CMD_START);
    for (i = 0; i < len; i++)
        outb(NE_BASE + NE_DATA, p[i]);
    for (; i < sz; i++)
        outb(NE_BASE + NE_DATA, 0);
    while ((rd(NE_ISR) & 0x40) == 0);
    wr(NE_ISR, 0x40);
}

static void rx_one(void) {
    ne_rx_hdr_t hdr;
    u8  buf[ETH_FRAME_MAX];
    u16 addr, len;
    u8  next;

    addr = (u16)rx_next << 8;
    dma_read(addr, &hdr, sizeof(hdr));

    next = hdr.next;
    len  = hdr.len - sizeof(hdr);

    if (len == 0 || len > ETH_FRAME_MAX) {
        rx_next = next;
        pg(1); wr(NE_P1_CURR, rx_next); pg(0);
        wr(NE_BNRY, (rx_next == NE_RX_START) ? NE_RX_STOP - 1 : rx_next - 1);
        return;
    }

    addr += sizeof(hdr);
    if (addr + len <= (u16)NE_RX_STOP << 8) {
        dma_read(addr, buf, len);
    } else {
        u16 part = ((u16)NE_RX_STOP << 8) - addr;
        dma_read(addr, buf, part);
        dma_read((u16)NE_RX_START << 8, buf + part, len - part);
    }

    rx_next = next;
    pg(1); wr(NE_P1_CURR, rx_next); pg(0);
    wr(NE_BNRY, (rx_next == NE_RX_START) ? NE_RX_STOP - 1 : rx_next - 1);

    eth_recv(buf, len);
}

static void ne_irq(registers_t *r) {
    u8 isr;
    (void)r;
    while ((isr = rd(NE_ISR)) & (NE_ISR_PRX | NE_ISR_RXE | NE_ISR_OVW)) {
        if (isr & NE_ISR_OVW) {
            wr(NE_CMD, NE_CMD_STOP | NE_CMD_RD2);
            pit_sleep(2);
            wr(NE_RBCR0, 0); wr(NE_RBCR1, 0);
            wr(NE_TCR, 0x02);
            wr(NE_CMD, NE_CMD_START | NE_CMD_RD2);
            pg(1);
            rx_next = rd(NE_P1_CURR);
            pg(0);
            wr(NE_BNRY, rx_next == NE_RX_START ? NE_RX_STOP - 1 : rx_next - 1);
            wr(NE_TCR, 0x00);
            wr(NE_ISR, NE_ISR_OVW);
        }
        if (isr & NE_ISR_PRX) {
            u8 curr;
            wr(NE_ISR, NE_ISR_PRX);
            pg(1); curr = rd(NE_P1_CURR); pg(0);
            while (rx_next != curr) {
                rx_one();
                pg(1); curr = rd(NE_P1_CURR); pg(0);
            }
        }
        if (isr & NE_ISR_RXE) wr(NE_ISR, NE_ISR_RXE);
    }
    isr = rd(NE_ISR);
    if (isr & NE_ISR_PTX) wr(NE_ISR, NE_ISR_PTX);
    if (isr & NE_ISR_TXE) wr(NE_ISR, NE_ISR_TXE);
    if (isr & NE_ISR_RST) wr(NE_ISR, NE_ISR_RST);
}

int ne2000_present(void) {
    u8 v;
    wr(NE_RESET, rd(NE_RESET));
    pit_sleep(10);
    v = rd(NE_ISR);
    return (v & NE_ISR_RST) != 0;
}

int ne2000_init(void) {
    u8 prom[32];
    u8 i;

    if (!ne2000_present()) return -1;

    wr(NE_ISR, 0xFF);
    wr(NE_CMD, NE_CMD_STOP | NE_CMD_RD2);
    pit_sleep(2);

    wr(NE_DCR,   NE_DCR_FT1 | NE_DCR_LS);
    wr(NE_RBCR0, 0); wr(NE_RBCR1, 0);
    wr(NE_RCR,   0x20);
    wr(NE_TCR,   0x02);
    wr(NE_TPSR,  NE_TX_PAGE);
    wr(NE_PSTART, NE_RX_START);
    wr(NE_PSTOP,  NE_RX_STOP);
    wr(NE_BNRY,   NE_RX_START);
    wr(NE_IMR,    0x00);
    wr(NE_ISR,    0xFF);

    dma_read(0, prom, 32);
    for (i = 0; i < ETH_ALEN; i++)
        mac[i] = prom[i * 2];

    pg(1);
    for (i = 0; i < ETH_ALEN; i++)
        wr(NE_P1_PAR0 + i, mac[i]);
    for (i = 0; i < 8; i++)
        wr(NE_P1_MAR0 + i, 0xFF);
    rx_next = NE_RX_START + 1;
    wr(NE_P1_CURR, rx_next);
    pg(0);

    wr(NE_CMD, NE_CMD_START | NE_CMD_RD2);
    wr(NE_TCR, 0x00);
    wr(NE_RCR, NE_RCR_AB | NE_RCR_AM);
    wr(NE_IMR, NE_ISR_PRX | NE_ISR_RXE | NE_ISR_OVW);

    idt_register_irq_handler(NE_IRQ, ne_irq);

    ready = 1;
    return 0;
}

int ne2000_send(const u8 *buf, u16 len) {
    u32 t;
    if (!ready) return -1;

    dma_write((u16)NE_TX_PAGE << 8, buf, len);

    wr(NE_TPSR,  NE_TX_PAGE);
    wr(NE_TBCR0, (len < 64 ? 64 : len) & 0xFF);
    wr(NE_TBCR1, (len < 64 ? 64 : len) >> 8);
    wr(NE_CMD,   NE_CMD_START | NE_CMD_TX | NE_CMD_RD2);

    t = 0;
    while (!(rd(NE_ISR) & (NE_ISR_PTX | NE_ISR_TXE)) && t++ < 100000);
    wr(NE_ISR, NE_ISR_PTX | NE_ISR_TXE);

    return (rd(NE_ISR) & NE_ISR_TXE) ? -1 : 0;
}

void ne2000_poll(void) {
    u8 curr;
    if (!ready) return;
    pg(1); curr = rd(NE_P1_CURR); pg(0);
    while (rx_next != curr) {
        rx_one();
        pg(1); curr = rd(NE_P1_CURR); pg(0);
    }
}

void ne2000_get_mac(u8 *out) {
    memcpy(out, mac, ETH_ALEN);
}
