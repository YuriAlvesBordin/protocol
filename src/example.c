#include <stdio.h>
#include <string.h>
#include "protocol.h"

/* Example demonstrating a WRITE then READ sequence between two nodes:
   Node 0x00 writes "Hello World!" to a register in Node 0x01,
   then Node 0x00 tries to read that register and prints the result.
*/

/* Buffers for simulated communication between two nodes */
#define BUFFER_SIZE 256
static uint8_t tx_buffers[2][BUFFER_SIZE];
static uint8_t rx_buffers[2][BUFFER_SIZE];
static size_t tx_lens[2] = {0, 0};
static size_t rx_lens[2] = {0, 0};
static size_t rx_pos[2] = {0, 0};

/* Track which node's protocol is currently active */
static int current_node = -1;

/* Simulated register in Node 0x01 */
static uint8_t reg[255];
static size_t reg_len = 0;

/* TX callback: append byte to the current node's transmit buffer */
static void tx_byte(uint8_t b)
{
    if (current_node < 0 || current_node >= 2) return;
    if (tx_lens[current_node] < BUFFER_SIZE)
    {
        tx_buffers[current_node][tx_lens[current_node]++] = b;
    }
}

/* RX callback: read byte from the current node's receive buffer */
static uint8_t rx_byte(void)
{
    if (current_node < 0 || current_node >= 2) return 0;
    if (rx_pos[current_node] < rx_lens[current_node])
    {
        return rx_buffers[current_node][rx_pos[current_node]++];
    }
    return 0; /* No data available */
}

/* Initialize the protocol for the current node */
static void proto_init_current(void)
{
    proto_init(tx_byte, rx_byte);
}

/* Helper to print payload as hex and ASCII */
static void print_payload(const char *label, const uint8_t *data, size_t len)
{
    printf("%s", label);
    for (size_t i = 0; i < len; i++)
    {
        printf("%02x ", data[i]);
    }
    printf(" (ASCII: \"");
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] >= 32 && data[i] <= 126)
            printf("%c", data[i]);
        else
            printf("<%02x>", data[i]);
    }
    printf("\")\n");
}

/* Simulate a transmission from node 'from' to node 'to' */
static void simulate_transmit(int from, int to, uint8_t addr, uint8_t broadcast, uint8_t type, const uint8_t *payload, size_t len)
{
    proto_result_t res;

    /* Step 1: Prepare the transmitter (node 'from') */
    current_node = from;
    proto_init_current();
    tx_lens[from] = 0; /* Clear transmit buffer */
    rx_lens[from] = 0; /* Clear receive buffer (not used for tx, but clean) */
    rx_pos[from] = 0;

    /* Step 2: Transmit the frame */
    res = proto_send_datagram(addr, broadcast, type, payload, len);
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x%02X: Failed to send datagram: %d\n", from, res);
        return;
    }
    printf("Node 0x%02X TX: ", from);
    for (size_t i = 0; i < tx_lens[from]; i++)
    {
        printf("%02x ", tx_buffers[from][i]);
    }
    printf("\n");

    /* Step 3: Copy the transmitted bytes to the receiver's buffer */
    tx_lens[to] = 0; /* Clear the receiver's transmit buffer (we don't want old tx data) */
    rx_lens[to] = tx_lens[from]; /* The receiver gets exactly what was transmitted */
    rx_pos[to] = 0; /* Start reading from the beginning */
    for (size_t i = 0; i < tx_lens[from]; i++)
    {
        rx_buffers[to][i] = tx_buffers[from][i];
    }
    printf("Node 0x%02X RX buffer loaded: ", to);
    for (size_t i = 0; i < rx_lens[to]; i++)
    {
        printf("%02x ", rx_buffers[to][i]);
    }
    printf("\n");

    /* Step 4: Prepare the receiver (node 'to') to process the incoming bytes */
    current_node = to;
    proto_init_current();
    /* Note: the receiver's transmit buffer is already cleared above */
}

int main(void)
{
    proto_result_t res;

    printf("=== Protocol WRITE/READ Example between Node 0x00 and Node 0x01 ===\n\n");

    /* Clear all buffers initially */
    for (int i = 0; i < 2; i++)
    {
        tx_lens[i] = 0;
        rx_lens[i] = 0;
        rx_pos[i] = 0;
    }

    /* ------------------- Phase 1: Node 0x00 writes to Node 0x01 ------------------- */
    printf("--- Phase 1: Node 0x00 writes \"Hello World!\" to Node 0x01 ---\n");

    const uint8_t write_payload[] = "Hello World!";
    const size_t write_payload_len = sizeof(write_payload) - 1; /* exclude null terminator */

    /* Simulate transmission from node 0x00 to node 0x01 */
    /* Address: 0x01 (destination node), Type: 0x01 (WRITE) */
    simulate_transmit(0, 1, 0x01, 0, 0x01, write_payload, write_payload_len);

    /* Now, node 0x01 should have the frame in its receive buffer.
       We need to let node 0x01 process it by calling proto_poll(). */
    current_node = 1;
    proto_init_current();
    /* We'll call proto_poll() enough times to process the entire frame */
    /* The frame length is: 3 (header) + write_payload_len + 2 (CRC) */
    int frame_len = 3 + write_payload_len + 2;
    for (int i = 0; i < frame_len * 2; i++) /* extra polls for safety */
    {
        proto_poll();
    }

    /* Check if node 0x01 received a frame */
    uint8_t addr, broadcast, type;
    uint8_t rx_payload[255];
    uint8_t len;
    res = proto_recv_datagram(&addr, &broadcast, &type, rx_payload, &len);
    if (res == PROTO_RESULT_OK && broadcast == 0 && addr == 0x01 && type == 0x01) /* WRITE */
    {
        /* Store the payload in the simulated register */
        memcpy(reg, rx_payload, len);
        reg_len = len;
        print_payload("Node 0x01 RX: Stored in register: ", reg, reg_len);
        print_payload("Node 0x01 RX: Stored in register: ", reg, reg_len);
    }
    else
    {
        printf("Node 0x01: Did not receive expected WRITE frame (res=%d, addr=0x%02x, broadcast=%d, type=%d)\n",
               res, addr, broadcast, type);
    }

    /* Clear node 0x01's receive buffer for the next phase */
    rx_lens[1] = 0;
    rx_pos[1] = 0;

    /* ------------------- Phase 2: Node 0x00 reads from Node 0x01 ------------------- */
    printf("\n--- Phase 2: Node 0x00 reads register from Node 0x01 ---\n");

    /* Node 0x00 sends a READ datagram to Node 0x01.
       We'll send a dummy register address (0x00) as payload (length 1). */
    const uint8_t read_addr = 0x00;
    /* Simulate transmission from node 0x00 to node 0x01 */
    /* Address: 0x01 (destination node), Type: 0x00 (READ) */
    simulate_transmit(0, 1, 0x01, 0, 0x00, &read_addr, 1);

    /* Now, node 0x01 should have the READ request in its receive buffer.
       Let node 0x01 process it. */
    current_node = 1;
    proto_init_current();
    int frame_len_read = 3 + 1 + 2; /* header + payload (1 byte) + CRC */
    for (int i = 0; i < frame_len_read * 2; i++)
    {
        proto_poll();
    }

    /* Check if node 0x01 received the READ request */
    res = proto_recv_datagram(&addr, &broadcast, &type, rx_payload, &len);
    if (res == PROTO_RESULT_OK && broadcast == 0 && addr == 0x01 && type == 0x00) /* READ */
    {
        /* Node 0x01 received a READ request; respond with the register contents */
        printf("Node 0x01 RX: Received READ request, sending register contents...\n");
        /* Simulate transmission from node 0x01 to node 0x00 */
        /* Address: 0x00 (destination node), Type: 0x01 (WRITE) */
        simulate_transmit(1, 0, 0x00, 0, 0x01, reg, reg_len);

        /* Now, node 0x00 should have the response in its receive buffer.
           Let node 0x00 process it. */
        current_node = 0;
        proto_init_current();
        int frame_len = 3 + reg_len + 2;
        for (int i = 0; i < frame_len * 2; i++)
        {
            proto_poll();
        }

        /* Check if node 0x00 received the response */
        res = proto_recv_datagram(&addr, &broadcast, &type, rx_payload, &len);
        if (res == PROTO_RESULT_OK && broadcast == 0 && addr == 0x00 && type == 0x01) /* WRITE */
        {
            print_payload("Node 0x00 RX: Received register contents: ", rx_payload, len);
            printf("\n=== RESULT: Node 0x00 successfully read \"");
            for (size_t i = 0; i < len; i++)
            {
                if (rx_payload[i] >= 32 && rx_payload[i] <= 126)
                    printf("%c", rx_payload[i]);
                else
                    printf("<%02x>", rx_payload[i]);
            }
            printf("\" from Node 0x01's register ===\n");
        }
        else
        {
            printf("Node 0x00: Did not receive expected response frame (res=%d, addr=0x%02x, broadcast=%d, type=%d)\n",
                   res, addr, broadcast, type);
        }
    }
    else
    {
        printf("Node 0x01: Did not receive expected READ frame (res=%d, addr=0x%02x, broadcast=%d, type=%d)\n",
               res, addr, broadcast, type);
    }

    return 0;
}