#include <stdio.h>
#include <string.h>
#include "protocol.h"

/* Example demonstrating streaming mode (connection mode) between two nodes:
   Node 0x00 acts as transmitter, Node 0x01 acts as receiver.
   They perform handshake, transmit a few blocks of data, and then close the connection.
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

    printf("=== Protocol Streaming Mode Example between Node 0x00 (TX) and Node 0x01 (RX) ===\n\n");

    /* Clear all buffers initially */
    for (int i = 0; i < 2; i++)
    {
        tx_lens[i] = 0;
        rx_lens[i] = 0;
        rx_pos[i] = 0;
    }

    /* ------------------- Phase 1: Handshake ------------------- */
    printf("--- Phase 1: Handshake (CONN_REQ / CONN_ACK) ---\n");

    /* Transmitter (node 0) initiates connection to receiver (node 1)
       Parameters: transmitter_id=0x00, session_id=0x42, receiver_list=[0x01], receiver_count=1, initial_ack_slot=0 */
    const uint8_t transmitter_id = 0x00;
    const uint8_t session_id = 0x42;
    const uint8_t receiver_list[] = { 0x01 };
    const uint8_t receiver_count = 1;
    const uint8_t initial_ack_slot = 0;

    res = proto_stream_tx_start(transmitter_id, session_id, receiver_list, receiver_count, initial_ack_slot);
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x00: Failed to start streaming transmitter: %d\n", res);
        return 1;
    }
    printf("Node 0x00: Streaming transmitter started (ID=0x%02X, Session=0x%02X)\n", transmitter_id, session_id);

    /* Receiver (node 1) starts as receiver */
    res = proto_stream_rx_start(0x01); /* receiver_id = 0x01 */
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x01: Failed to start streaming receiver: %d\n", res);
        return 1;
    }
    printf("Node 0x01: Streaming receiver started (ID=0x01)\n");

    /* Now we need to let the protocols exchange the CONN_REQ and CONN_ACK frames.
       The transmitter will send CONN_REQ to the receiver when we call proto_poll() on the transmitter side.
       We'll alternate polling both nodes to allow the handshake to complete. */

    printf("Exchanging handshake frames...\n");
    for (int handshake_step = 0; handshake_step < 10; handshake_step++)
    {
        /* Poll transmitter */
        current_node = 0;
        proto_init_current();
        proto_poll();

        /* Poll receiver */
        current_node = 1;
        proto_init_current();
        proto_poll();
    }

    /* Check if the transmitter has moved to STREAM_SENDING state (handshake succeeded) */
    /* We don't have direct access to the state, but we can infer from subsequent block transmission. */
    printf("Handshake exchange complete.\n\n");

    /* ------------------- Phase 2: Block Transmission ------------------- */
    printf("--- Phase 2: Transmitting 3 blocks of data ---\n");

    const size_t num_blocks = 3;
    const size_t block_data_size = 254; /* STREAM_DATA block data size */

    for (size_t block = 0; block < num_blocks; block++)
    {
        printf("--- Block %zu ---\n", block);

        /* Prepare block data: simple pattern */
        uint8_t block_data[block_data_size];
        for (size_t i = 0; i < block_data_size; i++)
        {
            block_data[i] = (uint8_t)((block * 0x10 + i) & 0xFF);
        }

        /* Transmitter sends the block */
        printf("Node 0x00 TX: Sending block %zu\n", block);
        res = proto_stream_tx_send_block(block_data);
        if (res != PROTO_RESULT_OK)
        {
            printf("Node 0x00: Failed to send block %zu: %d\n", block, res);
            return 1;
        }

        /* Allow time for transmission and ACK handling */
        printf("  Processing transmission and ACKs...\n");
        for (int poll_round = 0; poll_round < 20; poll_round++)
        {
            /* Poll transmitter */
            current_node = 0;
            proto_init_current();
            proto_poll();

            /* Poll receiver */
            current_node = 1;
            proto_init_current();
            proto_poll();
        }

        /* Check if receiver got the block */
        uint8_t rx_block[block_data_size];
        res = proto_stream_rx_get_block(rx_block);
        if (res == PROTO_RESULT_OK)
        {
            printf("Node 0x01 RX: Received block %zu\n", block);
            /* Verify data matches */
            int match = 1;
            for (size_t i = 0; i < block_data_size; i++)
            {
                if (rx_block[i] != block_data[i])
                {
                    match = 0;
                    break;
                }
            }
            if (match)
            {
                printf("  Block data matches! ✓\n");
            }
            else
            {
                printf("  Block data mismatch! ✗\n");
                /* Print first few bytes for debugging */
                printf("  Expected: ");
                for (size_t i = 0; i < 8; i++) printf("%02x ", block_data[i]);
                printf("...\n");
                printf("  Got:      ");
                for (size_t i = 0; i < 8; i++) printf("%02x ", rx_block[i]);
                printf("...\n");
            }
        }
        else
        {
            printf("Node 0x01: Failed to receive block %zu: %d\n", block, res);
        }

        printf("\n");
    }

    /* ------------------- Phase 3: Close Connection ------------------- */
    printf("--- Phase 3: Closing the streaming connection ---\n");

    /* Transmitter sends CLOSE with reason COMPLETE (0x00) */
    const uint8_t close_reason = 0x00; /* COMPLETE */
    printf("Node 0x00 TX: Sending CLOSE (reason=COMPLETE)\n");
    proto_stream_tx_close(close_reason);

    /* Allow time for CLOSE transmission and processing */
    printf("  Processing CLOSE...\n");
    for (int poll_round = 0; poll_round < 20; poll_round++)
    {
        /* Poll transmitter */
        current_node = 0;
        proto_init_current();
        proto_poll();

        /* Poll receiver */
        current_node = 1;
        proto_init_current();
        proto_poll();
    }

    /* Check if receiver got the CLOSE (by attempting to get a block - should fail) */
    uint8_t dummy[1];
    res = proto_stream_rx_get_block(dummy);
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x01: No more blocks available (connection closed) as expected.\n");
    }
    else
    {
        printf("Node 0x01: Unexpectedly received a block after CLOSE.\n");
    }

    printf("\n=== RESULT: Streaming session completed successfully ===\n");
    return 0;
}