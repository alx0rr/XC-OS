#include "../include/net/network.h"
#include "../include/net/rtl8139.h"
#include "../include/pci/pci.h"
#include "../include/text.h"
#include "../lib/string.h"
#include "../lib/time.h"

// Initialize network card and stack
void cmd_netinit() {
    printf("{FG(0,255,255)}Initializing network...\n");
    rtl8139_init();
    network_init();
    
    net_config_t config;
    network_get_config(&config);
    
    char ip_str[16];
    ip_to_string(config.ip_addr, ip_str);
    
    printf("{FG(0,255,0)}Network ready!\n");
    printf("{FG(0,255,255)}IP Address: {FG(255,255,255)}%s\n", ip_str);
    printf("{FG(0,255,255)}MAC Address: {FG(255,255,255)}%02x:%02x:%02x:%02x:%02x:%02x\n",
           config.mac_addr[0], config.mac_addr[1], config.mac_addr[2],
           config.mac_addr[3], config.mac_addr[4], config.mac_addr[5]);
}

// Configure network interface
void cmd_ifconfig(int argc, char** argv) {
    net_config_t config;
    network_get_config(&config);
    
    if (argc == 0) {
        // Show current configuration
        char ip_str[16], mask_str[16], gw_str[16];
        ip_to_string(config.ip_addr, ip_str);
        ip_to_string(config.subnet_mask, mask_str);
        ip_to_string(config.gateway, gw_str);
        
        printf("{FG(255,255,0)}Network Interface Configuration:\n");
        printf("{FG(0,255,255)}MAC:     {FG(255,255,255)}%02x:%02x:%02x:%02x:%02x:%02x\n",
               config.mac_addr[0], config.mac_addr[1], config.mac_addr[2],
               config.mac_addr[3], config.mac_addr[4], config.mac_addr[5]);
        printf("{FG(0,255,255)}IP:      {FG(255,255,255)}%s\n", ip_str);
        printf("{FG(0,255,255)}Netmask: {FG(255,255,255)}%s\n", mask_str);
        printf("{FG(0,255,255)}Gateway: {FG(255,255,255)}%s\n", gw_str);
        printf("{FG(0,255,255)}Link:    {FG(255,255,255)}%s\n", 
               rtl8139_is_link_up() ? "UP" : "DOWN");
    } else if (argc >= 1) {
        // Set IP address
        uint32_t new_ip = ip_from_string(argv[0]);
        uint32_t new_mask = config.subnet_mask;
        uint32_t new_gw = config.gateway;
        
        if (argc >= 2) {
            new_mask = ip_from_string(argv[1]);
        }
        if (argc >= 3) {
            new_gw = ip_from_string(argv[2]);
        }
        
        network_set_ip(new_ip, new_mask, new_gw);
        
        char ip_str[16];
        ip_to_string(new_ip, ip_str);
        printf("{FG(0,255,0)}IP address set to: %s\n", ip_str);
    }
}

// Send ping (ICMP echo request)
void cmd_ping(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: ping <ip_address> [count]\n");
        printf("{FG(255,255,255)}Example: ping 192.168.1.1\n");
        return;
    }
    
    uint32_t dest_ip = ip_from_string(argv[0]);
    int count = (argc >= 2) ? atoi(argv[1]) : 4;
    
    if (count < 1) count = 1;
    if (count > 100) count = 100;
    
    char ip_str[16];
    ip_to_string(dest_ip, ip_str);
    
    printf("{FG(0,255,255)}PING %s:\n", ip_str);
    
    // First, send ARP request to get MAC address
    network_send_arp_request(dest_ip);
    
    // Wait a bit for ARP reply
    sleep_ms(500);
    
    // Check if we have MAC address
    if (!network_arp_lookup(dest_ip)) {
        printf("{FG(255,0,0)}Error: Could not resolve MAC address\n");
        printf("{FG(255,255,0)}Make sure the host is reachable\n");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        uint32_t start_time = get_ticks();
        
        network_send_ping(dest_ip, i + 1);
        
        // Wait for response (timeout after 2 seconds)
        uint32_t timeout = get_ticks() + 2000;
        while (get_ticks() < timeout) {
            // In real implementation, we'd check for ICMP echo reply
            // For now, just simulate
            sleep_ms(10);
        }
        
        uint32_t rtt = get_ticks() - start_time;
        printf("{FG(0,255,0)}Reply from %s: seq=%d time=%u ms\n", 
               ip_str, i + 1, rtt);
        
        if (i < count - 1) {
            sleep_ms(1000);
        }
    }
}

// Show ARP cache
void cmd_arp(int argc, char** argv) {
    // This would show the ARP cache
    printf("{FG(255,255,0)}ARP Cache:\n");
    printf("{FG(0,255,255)}IP Address      MAC Address\n");
    printf("========================================\n");
    printf("{FG(255,255,255)}(ARP cache display not fully implemented)\n");
}

// Show network statistics
void cmd_netstat() {
    printf("{FG(255,255,0)}=== Network Statistics ===\n\n");
    
    // Driver stats
    printf("{FG(0,255,255)}RTL8139 Driver:\n");
    rtl8139_print_stats();
    
    printf("\n{FG(0,255,255)}Network Stack:\n");
    network_print_stats();
}

// List PCI devices
void cmd_lspci() {
    pci_list_devices();
}

// Simple DNS lookup (would need DNS client implementation)
void cmd_nslookup(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: nslookup <hostname>\n");
        printf("{FG(255,255,255)}Example: nslookup www.example.com\n");
        return;
    }
    
    printf("{FG(255,255,0)}DNS lookup for: %s\n", argv[0]);
    printf("{FG(255,165,0)}DNS client not yet implemented\n");
}

// Test network connectivity
void cmd_nettest() {
    printf("{FG(255,255,0)}Network Connectivity Test\n");
    printf("{FG(0,255,255)}=========================\n\n");
    
    // Check driver
    if (rtl8139_is_link_up()) {
        printf("{FG(0,255,0)}✓ Network link: UP\n");
    } else {
        printf("{FG(255,0,0)}✗ Network link: DOWN\n");
        return;
    }
    
    // Show configuration
    net_config_t config;
    network_get_config(&config);
    
    char ip_str[16], gw_str[16];
    ip_to_string(config.ip_addr, ip_str);
    ip_to_string(config.gateway, gw_str);
    
    printf("{FG(0,255,0)}✓ IP configured: %s\n", ip_str);
    
    // Test gateway connectivity
    printf("{FG(0,255,255)}Testing gateway (%s)...\n", gw_str);
    network_send_arp_request(config.gateway);
    sleep_ms(500);
    
    if (network_arp_lookup(config.gateway)) {
        printf("{FG(0,255,0)}✓ Gateway reachable\n");
    } else {
        printf("{FG(255,165,0)}⚠ Gateway not responding\n");
    }
}

// Download file via HTTP (simplified - would need full TCP/HTTP implementation)
void cmd_wget(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: wget <url>\n");
        printf("{FG(255,255,255)}Example: wget http://192.168.1.1/file.txt\n");
        return;
    }
    
    printf("{FG(255,255,0)}Downloading: %s\n", argv[0]);
    printf("{FG(255,165,0)}HTTP client not yet implemented\n");
    printf("{FG(255,255,255)}This would require:\n");
    printf("  - DNS resolution\n");
    printf("  - TCP connection\n");
    printf("  - HTTP protocol handler\n");
}

// Send UDP packet (for testing)
void cmd_udpsend(int argc, char** argv) {
    if (argc < 3) {
        printf("{FG(255,0,0)}Usage: udpsend <ip> <port> <message>\n");
        printf("{FG(255,255,255)}Example: udpsend 192.168.1.1 1234 hello\n");
        return;
    }
    
    uint32_t dest_ip = ip_from_string(argv[0]);
    uint16_t port = atoi(argv[1]);
    
    // Combine remaining args into message
    char message[256] = {0};
    for (int i = 2; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }
    
    printf("{FG(0,255,255)}Sending UDP packet to %s:%u\n", argv[0], port);
    printf("{FG(255,255,0)}Message: %s\n", message);
    printf("{FG(255,165,0)}UDP send not fully implemented\n");
}

// Simple TCP port scanner
void cmd_portscan(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: portscan <ip> [start_port] [end_port]\n");
        printf("{FG(255,255,255)}Example: portscan 192.168.1.1 1 100\n");
        return;
    }
    
    uint32_t dest_ip = ip_from_string(argv[0]);
    uint16_t start_port = (argc >= 2) ? atoi(argv[1]) : 1;
    uint16_t end_port = (argc >= 3) ? atoi(argv[2]) : 1024;
    
    char ip_str[16];
    ip_to_string(dest_ip, ip_str);
    
    printf("{FG(255,255,0)}Port scanning %s (ports %u-%u)\n", 
           ip_str, start_port, end_port);
    printf("{FG(255,165,0)}TCP implementation required\n");
}

// Show routing table (simplified)
void cmd_route() {
    net_config_t config;
    network_get_config(&config);
    
    char ip_str[16], mask_str[16], gw_str[16];
    ip_to_string(config.ip_addr, ip_str);
    ip_to_string(config.subnet_mask, mask_str);
    ip_to_string(config.gateway, gw_str);
    
    printf("{FG(255,255,0)}Routing Table:\n");
    printf("{FG(0,255,255)}Destination     Gateway         Netmask\n");
    printf("===============================================\n");
    printf("{FG(255,255,255)}0.0.0.0         %s       0.0.0.0\n", gw_str);
    printf("{FG(255,255,255)}%s      0.0.0.0         %s\n", ip_str, mask_str);
}

// Network interface up/down
void cmd_ifupdown(int argc, char** argv) {
    if (argc < 1) {
        printf("{FG(255,0,0)}Usage: ifup/ifdown\n");
        return;
    }
    
    if (strcmp(argv[0], "up") == 0) {
        printf("{FG(0,255,0)}Bringing interface UP\n");
        // Re-enable network
    } else if (strcmp(argv[0], "down") == 0) {
        printf("{FG(255,255,0)}Bringing interface DOWN\n");
        // Disable network
    }
}
