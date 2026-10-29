// udp_server.h
// Module which listens for UDP commands on port 12345 (in its own thread) and
// replies to the sender. See the assignment (section 1.2) for the commands:
//   help / ?, count, length, dips, history, stop, <enter> (repeat last).
//
// All light data comes from the sampler module; this module never rotates the
// history (terminal_out does that once per second), it only reads it.
//
// Typical use in main():
//   UdpServer_init();
//   UdpServer_waitForStop();     // blocks until a "stop" command arrives
//   UdpServer_cleanup();         // ...then shut everything down in reverse order

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

// Open the socket and start the listening thread.
void UdpServer_init(void);

// Stop the thread (if still running), close the socket.
void UdpServer_cleanup(void);

// Block the calling thread until a "stop" command has been received.
void UdpServer_waitForStop(void);

#endif