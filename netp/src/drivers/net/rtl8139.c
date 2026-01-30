#include "../../include/net/rtl8139.h"
#include "../../include/net/network.h"
#include "../../include/pci/pci.h"
#include "../../include/memory/pmm.h"
#include "../../include/interrupts/idt.h"
#include "../../include/text.h"
#include "../../lib/io.h"
#include "../../lib/string.h"
#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define RX_BUF_STATIC_ADDR 0x01400000
#define TX_BUF0_STATIC_ADDR 0x01500000
#define TX_BUF1_STATIC_ADDR 0x01502000
#define TX_BUF2_STATIC_ADDR 0x01504000
#define TX_BUF3_STATIC_ADDR 0x01506000

static rtl8139_device_t rtl8139_dev;
static uint8_t rtl8139_initialized = 0;

static void rtl8139_reset(void) {
    outb(rtl8139_dev.io_base + RTL8139_CHIPCMD, RTL8139_CMD_RESET);
    int timeout = 1000;
    while ((inb(rtl8139_dev.io_base + RTL8139_CHIPCMD) & RTL8139_CMD_RESET) && timeout > 0) {
        timeout--;
        for (volatile int i = 0; i < 1000; i++);
    }
    if (timeout == 0) {
        printf("{FG(255,0,0)}RTL8139: Reset timeout\n");
    }
}

static void rtl8139_read_mac(void) {
    for (int i = 0; i < 6; i++) {
        rtl8139_dev.mac_addr[i] = inb(rtl8139_dev.io_base + RTL8139_IDR0 + i);
    }
    printf("{FG(0,255,0)}RTL8139: MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           rtl8139_dev.mac_addr[0], rtl8139_dev.mac_addr[1],
           rtl8139_dev.mac_addr[2], rtl8139_dev.mac_addr[3],
           rtl8139_dev.mac_addr[4], rtl8139_dev.mac_addr[5]);
}

static void rtl8139_setup_buffers(void) {
    rtl8139_dev.rx_buffer = (uint8_t*)RX_BUF_STATIC_ADDR;
    rtl8139_dev.rx_buffer_phys = RX_BUF_STATIC_ADDR;
    
    for (uint32_t i = 0; i < RTL8139_RX_BUF_SIZE + 2048; i++) {
        rtl8139_dev.rx_buffer[i] = 0;
    }
    
    rtl8139_dev.tx_buffer[0] = (uint8_t*)TX_BUF0_STATIC_ADDR;
    rtl8139_dev.tx_buffer[1] = (uint8_t*)TX_BUF1_STATIC_ADDR;
    rtl8139_dev.tx_buffer[2] = (uint8_t*)TX_BUF2_STATIC_ADDR;
    rtl8139_dev.tx_buffer[3] = (uint8_t*)TX_BUF3_STATIC_ADDR;
    
    rtl8139_dev.tx_buffer_phys[0] = TX_BUF0_STATIC_ADDR;
    rtl8139_dev.tx_buffer_phys[1] = TX_BUF1_STATIC_ADDR;
    rtl8139_dev.tx_buffer_phys[2] = TX_BUF2_STATIC_ADDR;
    rtl8139_dev.tx_buffer_phys[3] = TX_BUF3_STATIC_ADDR;
    
    for (int i = 0; i < RTL8139_NUM_TX_DESC; i++) {
        for (uint32_t j = 0; j < RTL8139_TX_BUF_SIZE; j++) {
            rtl8139_dev.tx_buffer[i][j] = 0;
        }
    }
    
    outl(rtl8139_dev.io_base + RTL8139_RXBUF, rtl8139_dev.rx_buffer_phys);
    for (int i = 0; i < RTL8139_NUM_TX_DESC; i++) {
        outl(rtl8139_dev.io_base + RTL8139_TXADDR0 + (i * 4), rtl8139_dev.tx_buffer_phys[i]);
    }
    
    rtl8139_dev.rx_offset = 0;
    rtl8139_dev.tx_current = 0;
}

static void rtl8139_configure(void) {
    outb(rtl8139_dev.io_base + RTL8139_CHIPCMD, 
         RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);
    
    uint32_t rx_config = RTL8139_RX_AB | RTL8139_RX_APM | RTL8139_RX_WRAP;
    rx_config |= (7 << RTL8139_RX_MXDMA_SHIFT);
    rx_config |= (0 << RTL8139_RX_RBLEN_SHIFT);
    rx_config |= (6 << RTL8139_RX_FTH_SHIFT);
    outl(rtl8139_dev.io_base + RTL8139_RXCONFIG, rx_config);
    
    uint32_t tx_config = (7 << RTL8139_TX_MXDMA_SHIFT);
    tx_config |= (0x03 << RTL8139_TX_RETRYCNT_SHIFT);
    outl(rtl8139_dev.io_base + RTL8139_TXCONFIG, tx_config);
    
    uint16_t int_mask = RTL8139_INT_RXOK | RTL8139_INT_RXERR | 
                        RTL8139_INT_TXOK | RTL8139_INT_TXERR |
                        RTL8139_INT_RXOVW | RTL8139_INT_LINKCHG;
    outw(rtl8139_dev.io_base + RTL8139_INTRMASK, int_mask);
    outw(rtl8139_dev.io_base + RTL8139_INTRSTATUS, 0xFFFF);
}

void rtl8139_init(void) {
    printf("{FG(0,255,255)}Initializing RTL8139 network card...\n");
    pci_init();
    
    pci_device_t* pci_dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!pci_dev) {
        printf("{FG(255,0,0)}RTL8139: Device not found\n");
        return;
    }
    
    printf("{FG(0,255,0)}RTL8139: Found at bus %u, device %u, function %u\n",
           pci_dev->bus, pci_dev->device, pci_dev->function);
    
    rtl8139_dev.io_base = pci_dev->bar[0] & 0xFFFFFFF0;
    rtl8139_dev.irq = pci_dev->interrupt_line;
    
    printf("{FG(0,255,0)}RTL8139: I/O Base: 0x%x, IRQ: %u\n", 
           rtl8139_dev.io_base, rtl8139_dev.irq);
    
    pci_enable_bus_mastering(pci_dev);
    outb(rtl8139_dev.io_base + RTL8139_CONFIG1, 0x00);
    
    rtl8139_reset();
    rtl8139_read_mac();
    rtl8139_setup_buffers();
    rtl8139_configure();
    
    idt_register_irq_handler(rtl8139_dev.irq, (void*)rtl8139_irq_handler);
    
    rtl8139_dev.rx_packets = 0;
    rtl8139_dev.tx_packets = 0;
    rtl8139_dev.rx_bytes = 0;
    rtl8139_dev.tx_bytes = 0;
    rtl8139_dev.rx_errors = 0;
    rtl8139_dev.tx_errors = 0;
    rtl8139_dev.link_up = 1;
    
    rtl8139_initialized = 1;
    printf("{FG(0,255,0)}RTL8139: Initialization complete\n");
}

int rtl8139_send_packet(const void* data, uint32_t len) {
    if (!rtl8139_initialized) return -1;
    if (len > RTL8139_TX_BUF_SIZE) return -1;
    
    uint8_t tx_desc = rtl8139_dev.tx_current;
    
    int timeout = 10000;
    while (timeout > 0) {
        uint32_t status = inl(rtl8139_dev.io_base + RTL8139_TXSTATUS0 + (tx_desc * 4));
        if (status & (RTL8139_TX_STAT_OK | RTL8139_TX_ABORTED)) {
            break;
        }
        timeout--;
        for (volatile int i = 0; i < 100; i++);
    }
    
    if (timeout == 0) {
        rtl8139_dev.tx_errors++;
        return -1;
    }
    
    memcpy(rtl8139_dev.tx_buffer[tx_desc], data, len);
    outl(rtl8139_dev.io_base + RTL8139_TXSTATUS0 + (tx_desc * 4), len);
    
    rtl8139_dev.tx_packets++;
    rtl8139_dev.tx_bytes += len;
    
    rtl8139_dev.tx_current = (rtl8139_dev.tx_current + 1) % RTL8139_NUM_TX_DESC;
    
    return 0;
}

void rtl8139_receive_packet(void) {
    uint16_t rx_status = inw(rtl8139_dev.io_base + RTL8139_RXBUFTAIL);
    
    while ((inb(rtl8139_dev.io_base + RTL8139_CHIPCMD) & RTL8139_CMD_BUF_EMPTY) == 0) {
        uint16_t* header = (uint16_t*)(rtl8139_dev.rx_buffer + rtl8139_dev.rx_offset);
        uint16_t rx_flags = header[0];
        uint16_t rx_len = header[1];
        
        if (!(rx_flags & 0x0001) || (rx_len > RTL8139_RX_BUF_SIZE)) {
            rtl8139_dev.rx_errors++;
            rtl8139_reset();
            rtl8139_setup_buffers();
            rtl8139_configure();
            return;
        }
        
        uint8_t* packet = (uint8_t*)(header + 2);
        uint32_t packet_len = rx_len - 4;
        
        rtl8139_dev.rx_packets++;
        rtl8139_dev.rx_bytes += packet_len;
        
        network_process_packet(packet, packet_len);
        
        rtl8139_dev.rx_offset = (rtl8139_dev.rx_offset + rx_len + 4 + 3) & ~3;
        if (rtl8139_dev.rx_offset >= RTL8139_RX_BUF_SIZE) {
            rtl8139_dev.rx_offset -= RTL8139_RX_BUF_SIZE;
        }
        
        outw(rtl8139_dev.io_base + RTL8139_RXBUFTAIL, rtl8139_dev.rx_offset - 16);
    }
}

void rtl8139_irq_handler(void) {
    if (!rtl8139_initialized) return;
    
    uint16_t status = inw(rtl8139_dev.io_base + RTL8139_INTRSTATUS);
    outw(rtl8139_dev.io_base + RTL8139_INTRSTATUS, status);
    
    if (status & RTL8139_INT_RXOK) {
        rtl8139_receive_packet();
    }
    
    if (status & RTL8139_INT_RXERR) {
        rtl8139_dev.rx_errors++;
    }
    
    if (status & RTL8139_INT_TXERR) {
        rtl8139_dev.tx_errors++;
    }
    
    if (status & RTL8139_INT_RXOVW) {
        rtl8139_dev.rx_errors++;
        printf("{FG(255,165,0)}RTL8139: RX buffer overflow\n");
    }
    
    if (status & RTL8139_INT_LINKCHG) {
        uint8_t msr = inb(rtl8139_dev.io_base + RTL8139_MSR);
        rtl8139_dev.link_up = (msr & 0x04) ? 0 : 1;
        printf("{FG(255,255,0)}RTL8139: Link %s\n", 
               rtl8139_dev.link_up ? "UP" : "DOWN");
    }
}

void rtl8139_get_mac_address(uint8_t* mac) {
    if (!rtl8139_initialized || !mac) return;
    memcpy(mac, rtl8139_dev.mac_addr, 6);
}

uint8_t rtl8139_is_link_up(void) {
    return rtl8139_initialized ? rtl8139_dev.link_up : 0;
}

void rtl8139_print_stats(void) {
    printf("{FG(255,255,0)}RTL8139 Statistics:\n");
    printf("{FG(0,255,255)}RX Packets: {FG(255,255,255)}%u\n", rtl8139_dev.rx_packets);
    printf("{FG(0,255,255)}RX Bytes:   {FG(255,255,255)}%u\n", rtl8139_dev.rx_bytes);
    printf("{FG(0,255,255)}RX Errors:  {FG(255,255,255)}%u\n", rtl8139_dev.rx_errors);
    printf("{FG(0,255,255)}TX Packets: {FG(255,255,255)}%u\n", rtl8139_dev.tx_packets);
    printf("{FG(0,255,255)}TX Bytes:   {FG(255,255,255)}%u\n", rtl8139_dev.tx_bytes);
    printf("{FG(0,255,255)}TX Errors:  {FG(255,255,255)}%u\n", rtl8139_dev.tx_errors);
    printf("{FG(0,255,255)}Link:       {FG(255,255,255)}%s\n", 
           rtl8139_dev.link_up ? "UP" : "DOWN");
}
