// Temporary test main: sampler + terminal output + UDP server.
// Run it, then from the host:  netcat -u 192.168.7.2 12345
#include <stdio.h>
 
#include "periodTimer.h"
#include "sampler.h"
#include "terminal_out.h"
#include "udp_server.h"
#include "led_control.h"
 
int main(void)
{
    printf("Starting light sampler...\n");
 
    Period_init();
    Sampler_init();
    TerminalOut_init();
    UdpServer_init();
    LedControl_init();
 
    // TerminalOut_setFlashHz(10);     // later: set by the rotary encoder code
 
    UdpServer_waitForStop();        // returns after a "stop" command
 
    // Shut down in reverse order.
    UdpServer_cleanup();
    LedControl_cleanup();
    TerminalOut_cleanup();
    Sampler_cleanup();
    Period_cleanup();
 
    printf("Done.\n");
    return 0;
}
