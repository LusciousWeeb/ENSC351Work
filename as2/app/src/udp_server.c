// udp_server.c
// Listens on UDP port 12345 and answers the light-sampler commands.

#include "udp_server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sampler.h"

#define UDP_PORT            12345
#define RX_BUFFER_SIZE      1024
#define POLL_TIMEOUT_MS     100     // how often the thread checks for shutdown

// A UDP packet can hold 1500 bytes; stay safely under that.
#define MAX_PACKET_BYTES    1400
#define VALUES_PER_LINE     10

typedef enum {
    CMD_NONE,       // nothing received yet (only used for "last command")
    CMD_BLANK,      // empty line: repeat the previous command
    CMD_HELP,
    CMD_COUNT,
    CMD_LENGTH,
    CMD_DIPS,
    CMD_HISTORY,
    CMD_STOP,
    CMD_UNKNOWN
} command_t;

static const char HELP_TEXT[] =
    "\nAccepted command examples:\n"
    "count       -- get the total number of samples taken.\n"
    "length      -- get the number of samples taken in the previously completed second.\n"
    "dips        -- get the number of dips in the previously completed second.\n"
    "history     -- get all the samples in the previously completed second.\n"
    "stop        -- cause the server program to end.\n"
    "<enter>     -- repeat last command.\n";

static const char UNKNOWN_TEXT[] =
    "Unknown command. Type 'help' for the list of commands.\n";

static pthread_t s_thread;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_stopCond = PTHREAD_COND_INITIALIZER;
static bool s_isInitialized = false;
static bool s_isRunning = false;            // protected by s_lock
static bool s_stopRequested = false;        // protected by s_lock
static int s_socket = -1;


// ---------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------
static void sendReply(const struct sockaddr_in *dest, const char *text, size_t length)
{
    ssize_t sent = sendto(s_socket, text, length, 0,
                          (const struct sockaddr *)dest, sizeof(*dest));
    if (sent < 0) {
        perror("UdpServer: sendto");
    }
}

static void sendString(const struct sockaddr_in *dest, const char *text)
{
    sendReply(dest, text, strlen(text));
}

// Send the previous second's samples: 10 per line, comma separated, 3
// decimals. Lines are packed into packets without splitting a line (so a
// sample's digits are never split across packets).
static void sendHistory(const struct sockaddr_in *dest)
{
    int size = 0;
    double *history = Sampler_getHistory(&size);

    if (size == 0) {
        sendString(dest, "No samples in the previous second yet.\n");
        free(history);
        return;
    }

    char packet[MAX_PACKET_BYTES];
    size_t packetLen = 0;

    for (int start = 0; start < size; start += VALUES_PER_LINE) {
        // Build one line of up to 10 values.
        char line[128];
        size_t lineLen = 0;
        for (int i = start; i < start + VALUES_PER_LINE && i < size; i++) {
            bool isLastOfAll = (i == size - 1);
            bool isLastOfLine = ((i + 1) % VALUES_PER_LINE == 0) || isLastOfAll;

            int written = snprintf(line + lineLen, sizeof(line) - lineLen,
                                   "%.3f%s%s",
                                   history[i],
                                   isLastOfAll ? "" : ",",
                                   isLastOfLine ? "\n" : " ");
            if (written < 0 || (size_t)written >= sizeof(line) - lineLen) {
                break;      // can't happen for sane voltages; don't overflow
            }
            lineLen += (size_t)written;
        }

        // Flush the packet first if this line wouldn't fit.
        if (packetLen + lineLen > sizeof(packet)) {
            sendReply(dest, packet, packetLen);
            packetLen = 0;
        }
        memcpy(packet + packetLen, line, lineLen);
        packetLen += lineLen;
    }

    if (packetLen > 0) {
        sendReply(dest, packet, packetLen);
    }
    free(history);
}


// ---------------------------------------------------------------------------
// Parsing and running commands
// ---------------------------------------------------------------------------
static command_t parseCommand(const char *text)
{
    // Trim leading/trailing whitespace (including the \n or \r\n) and
    // lower-case, so "HELP\r\n" and "help" are both accepted.
    char word[32];
    size_t len = 0;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    for (; *text != '\0' && len < sizeof(word) - 1; text++) {
        char c = *text;
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        word[len++] = c;
    }
    while (len > 0 && (word[len - 1] == ' ' || word[len - 1] == '\t')) {
        len--;
    }
    word[len] = '\0';

    if (len == 0)                       return CMD_BLANK;
    if (strcmp(word, "help") == 0)      return CMD_HELP;
    if (strcmp(word, "?") == 0)         return CMD_HELP;
    if (strcmp(word, "count") == 0)     return CMD_COUNT;
    if (strcmp(word, "length") == 0)    return CMD_LENGTH;
    if (strcmp(word, "dips") == 0)      return CMD_DIPS;
    if (strcmp(word, "history") == 0)   return CMD_HISTORY;
    if (strcmp(word, "stop") == 0)      return CMD_STOP;
    return CMD_UNKNOWN;
}

// Run one command. Returns false if the server should stop.
static bool runCommand(command_t command, const struct sockaddr_in *dest)
{
    char reply[128];

    switch (command) {
    case CMD_HELP:
        sendString(dest, HELP_TEXT);
        break;
    case CMD_COUNT:
        snprintf(reply, sizeof(reply), "# samples taken total: %lld\n",
                 Sampler_getNumSamplesTaken());
        sendString(dest, reply);
        break;
    case CMD_LENGTH:
        snprintf(reply, sizeof(reply), "# samples taken last second: %d\n",
                 Sampler_getHistorySize());
        sendString(dest, reply);
        break;
    case CMD_DIPS:
        snprintf(reply, sizeof(reply), "# Dips: %d\n", Sampler_getDipCount());
        sendString(dest, reply);
        break;
    case CMD_HISTORY:
        sendHistory(dest);
        break;
    case CMD_STOP:
        sendString(dest, "Program terminating.\n");
        return false;
    case CMD_NONE:
    case CMD_BLANK:
    case CMD_UNKNOWN:
    default:
        sendString(dest, UNKNOWN_TEXT);
        break;
    }
    return true;
}


// ---------------------------------------------------------------------------
// Thread
// ---------------------------------------------------------------------------
static void *udpThread(void *arg)
{
    (void)arg;

    command_t lastCommand = CMD_NONE;
    bool keepGoing = true;

    while (keepGoing) {
        // Wait for a packet, but wake up regularly to check for shutdown.
        struct pollfd pfd = { .fd = s_socket, .events = POLLIN, .revents = 0 };
        int ready = poll(&pfd, 1, POLL_TIMEOUT_MS);

        pthread_mutex_lock(&s_lock);
        bool running = s_isRunning;
        pthread_mutex_unlock(&s_lock);
        if (!running) {
            break;
        }
        if (ready <= 0) {
            continue;       // timeout (or interrupted): check again
        }

        char rx[RX_BUFFER_SIZE];
        struct sockaddr_in sender;
        socklen_t senderLen = sizeof(sender);
        ssize_t bytes = recvfrom(s_socket, rx, sizeof(rx) - 1, 0,
                                 (struct sockaddr *)&sender, &senderLen);
        if (bytes < 0) {
            perror("UdpServer: recvfrom");
            continue;
        }
        rx[bytes] = '\0';

        command_t command = parseCommand(rx);
        if (command == CMD_BLANK) {
            // Blank line repeats the previous command; if there isn't one,
            // it's treated as unknown.
            command = lastCommand;
        } else {
            lastCommand = command;
        }

        keepGoing = runCommand(command, &sender);
    }

    // Reached by "stop" (keepGoing false): tell whoever is waiting in main.
    pthread_mutex_lock(&s_lock);
    if (!keepGoing) {
        s_stopRequested = true;
    }
    pthread_cond_broadcast(&s_stopCond);
    pthread_mutex_unlock(&s_lock);
    return NULL;
}


// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------
void UdpServer_init(void)
{
    assert(!s_isInitialized);

    s_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_socket < 0) {
        perror("ERROR: UdpServer: unable to create socket");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(s_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(UDP_PORT);
    if (bind(s_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("ERROR: UdpServer: unable to bind port 12345");
        close(s_socket);
        exit(EXIT_FAILURE);
    }

    s_isRunning = true;
    s_stopRequested = false;
    s_isInitialized = true;

    if (pthread_create(&s_thread, NULL, udpThread, NULL) != 0) {
        perror("ERROR: UdpServer: unable to create thread");
        exit(EXIT_FAILURE);
    }
}

void UdpServer_waitForStop(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    while (!s_stopRequested) {
        pthread_cond_wait(&s_stopCond, &s_lock);
    }
    pthread_mutex_unlock(&s_lock);
}

void UdpServer_cleanup(void)
{
    assert(s_isInitialized);

    pthread_mutex_lock(&s_lock);
    s_isRunning = false;
    pthread_mutex_unlock(&s_lock);

    pthread_join(s_thread, NULL);

    close(s_socket);
    s_socket = -1;
    s_isInitialized = false;
}