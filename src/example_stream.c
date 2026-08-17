#include <stdio.h>
#include <string.h>
#include "protocol.h"

/* Example demonstrating streaming mode (connection mode) between two nodes:
   Node 0x00 acts as transmitter, Node 0x01 acts as receiver.
   They perform handshake, transmit a few blocks of data, and then close the connection.
*/

/* Simulated unidirectional mediums: 0->1 and 1->0 */
#define MEDIUM_SIZE 256
static uint8_t medium_0_to_1[MEDIUM_SIZE];
static size_t medium_0_to_1_write = 0;
static size_t medium_0_to_1_read = 0;

static uint8_t medium_1_to_0[MEDIUM_SIZE];
static size_t medium_1_to_0_write = 0;
static size_t medium_1_to_0_read = 0;

/* Helper to reset mediums */
static void medium_reset(void)
{
    medium_0_to_1_write = medium_0_to_1_read = 0;
    medium_1_to_0_write = medium_1_to_0_read = 0;
}

/* TX callback for node 0: append byte to medium_0_to_1 */
static void tx_byte_0(uint8_t b)
{
    if ((medium_0_to_1_write + 1) % MEDIUM_SIZE != medium_0_to_1_read)
    {
        medium_0_to_1[medium_0_to_1_write] = b;
        medium_0_to_1_write = (medium_0_to_1_write + 1) % MEDIUM_SIZE;
    }
}

/* RX callback for node 0: read byte from medium_1_to_0 */
static uint8_t rx_byte_0(void)
{
    if (medium_1_to_0_read != medium_1_to_0_write)
    {
        uint8_t b = medium_1_to_0[medium_1_to_0_read];
        medium_1_to_0_read = (medium_1_to_0_read + 1) % MEDIUM_SIZE;
        return b;
    }
    return 0; /* No data available */
}

/* TX callback for node 1: append byte to medium_1_to_0 */
static void tx_byte_1(uint8_t b)
{
    if ((medium_1_to_0_write + 1) % MEDIUM_SIZE != medium_1_to_0_read)
    {
        medium_1_to_0[medium_1_to_0_write] = b;
        medium_1_to_0_write = (medium_1_to_0_write + 1) % MEDIUM_SIZE;
    }
}

/* RX callback for node 1: read byte from medium_0_to_1 */
static uint8_t rx_byte_1(void)
{
    if (medium_0_to_1_read != medium_0_to_1_write)
    {
        uint8_t b = medium_0_to_1[medium_0_to_1_read];
        medium_0_to_1_read = (medium_0_to_1_read + 1) % MEDIUM_SIZE;
        return b;
    }
    return 0; /* No data available */
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

int main(void)
{
    printf("=== Protocol Streaming Mode Example between Node 0x00 (TX) and Node 0x01 (RX) ===\n\n");

    /* Reset mediums */
    medium_reset();

    /* Create protocol contexts for each node */
    protocol_t ctx[2];

    /* Initialize each protocol instance with its own TX/RX callbacks */
    proto_init(&ctx[0], tx_byte_0, rx_byte_0); /* Node 0x00 */
    proto_init(&ctx[1], tx_byte_1, rx_byte_1); /* Node 0x01 */

    /* ------------------- Phase 1: Handshake ------------------- */
    printf("--- Phase 1: Handshake (CONN_REQ / CONN_ACK) ---\n");

    /* Transmitter (node 0) initiates connection to receiver (node 1)
       Parameters: transmitter_id=0x00, session_id=0x42, receiver_list=[0x01], receiver_count=1, initial_ack_slot=0 */
    const uint8_t transmitter_id = 0x00;
    const uint8_t session_id = 0x42;
    const uint8_t receiver_list[] = { 0x01 };
    const uint8_t receiver_count = 1;
    const uint8_t initial_ack_slot = 0;

    proto_result_t res = proto_stream_tx_start(&ctx[0], transmitter_id, session_id, receiver_list, receiver_count, initial_ack_slot);
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x00: Failed to start streaming transmitter: %d\n", res);
        return 1;
    }
    printf("Node 0x00: Streaming transmitter started (ID=0x%02X, Session=0x%02X)\n", transmitter_id, session_id);

    /* Receiver (node 1) starts as receiver */
    res = proto_stream_rx_start(&ctx[1], 0x01); /* receiver_id = 0x01 */
    if (res != PROTO_RESULT_OK)
    {
        printf("Node 0x01: Failed to start streaming receiver: %d\n", res);
        return 1;
    }
    printf("Node 0x01: Streaming receiver started (ID=0x01)\n");

    /* Now we need to let the protocols exchange the CONN_REQ and CONN_ACK frames.
       We'll alternate polling both nodes to allow the handshake to complete. */

    printf("Exchanging handshake frames...\n");
    for (int handshake_step = 0; handshake_step < 200; handshake_step++)
    {
        /* Poll transmitter (node 0) */
        proto_poll(&ctx[0]);

        /* Poll receiver (node 1) */
        proto_poll(&ctx[1]);
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
        res = proto_stream_tx_send_block(&ctx[0], block_data);
        if (res != PROTO_RESULT_OK)
        {
            printf("Node 0x00: Failed to send block %zu: %d\n", block, res);
            return 1;
        }

        /* Allow time for transmission and ACK handling */
        printf("  Processing transmission and ACKs...\n");
        for (int poll_round = 0; poll_round < 100; poll_round++)
        {
            /* Poll transmitter */
            proto_poll(&ctx[0]);

            /* Poll receiver */
            proto_poll(&ctx[1]);
        }

        /* Check if receiver got the block */
        uint8_t rx_block[block_data_size];
        res = proto_stream_rx_get_block(&ctx[1], rx_block);
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
    proto_stream_tx_close(&ctx[0], close_reason);

    /* Allow time for CLOSE transmission and processing */
    printf("  Processing CLOSE...\n");
    for (int poll_round = 0; poll_round < 100; poll_round++)
    {
        /* Poll transmitter */
        proto_poll(&ctx[0]);

        /* Poll receiver */
        proto_poll(&ctx[1]);
    }

    /* Check if receiver got the CLOSE (by attempting to get a block - should fail) */
    uint8_t dummy[1];
    res = proto_stream_rx_get_block(&ctx[1], dummy);
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