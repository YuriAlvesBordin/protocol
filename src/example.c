#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"

/* Example showing how to override configuration values */
/* Uncomment the following lines to override default values before including protocol_config.h */
/*
#define PROTO_MAX_PAYLOAD_LENGTH      128u    // Reduce max payload to 128 bytes
#define STREAM_TX_TIMEOUT_CONN        200u    // Increase connection timeout
#define STREAM_TX_TIMEOUT_ACK         100u    // Increase ACK timeout
#define STREAM_RX_TIMEOUT_ACK_TURN    100u    // Increase ACK turn timeout
#define STREAM_RX_TIMEOUT_WAITING     2000u   // Increase waiting timeout
*/
#include "protocol_config.h"

/* Simple example showing how to use the protocol for datagram communication */

/* Separate buffers for TX and RX to avoid interference */
static uint8_t tx_buffer[256];
static uint8_t rx_buffer[256];
static size_t tx_len = 0;
static size_t rx_len = 0;
static size_t rx_pos = 0;

/* Simple transmit function - sends data to "receiver" */
static void tx_byte(uint8_t b)
{
    if (tx_len < sizeof(tx_buffer))
    {
        tx_buffer[tx_len++] = b;
    }
}

/* Simple receive function - gets data from "transmitter" */
static uint8_t rx_byte(void)
{
    // If we have unread data in rx_buffer, return it
    if (rx_pos < rx_len)
    {
        return rx_buffer[rx_pos++];
    }
    
    // Otherwise, check if there's new data in tx_buffer to process
    if (tx_len > 0)
    {
        // Copy all data from tx_buffer to rx_buffer
        if (rx_len + tx_len <= sizeof(rx_buffer))
        {
            memcpy(&rx_buffer[rx_len], tx_buffer, tx_len);
            rx_len += tx_len;
        }
        
        // Clear tx_buffer since we've "transmitted" the data
        tx_len = 0;
        
        // Now return the first byte if available
        if (rx_pos < rx_len)
        {
            return rx_buffer[rx_pos++];
        }
    }
    
    return 0; /* Indicate no data available */
}

int main(void)
{
    printf("=== Protocol Datagram Example (Loop Mode) ===\n");
    printf("Configuration:\n");
    printf("  Max Payload Length: %u bytes\n", PROTO_MAX_PAYLOAD_LENGTH);
    printf("  Frame Buffer Size: %u bytes\n", PROTO_MAX_PAYLOAD_LENGTH + 5);
    printf("  Stream TX Max Receivers: %u\n", STREAM_TX_MAX_RECEIVERS);
    printf("  Stream TX Timeout Conn: %u polls\n", STREAM_TX_TIMEOUT_CONN);
    printf("  Stream TX Timeout Ack: %u polls\n", STREAM_TX_TIMEOUT_ACK);
    printf("  Stream TX Max Retries: %u\n", STREAM_TX_MAX_RETRIES);
    printf("  Stream RX Timeout Ack Turn: %u polls\n", STREAM_RX_TIMEOUT_ACK_TURN);
    printf("  Stream RX Timeout Waiting: %u polls\n", STREAM_RX_TIMEOUT_WAITING);
    printf("\n");

    /* Initialize the protocol */
    proto_init(tx_byte, rx_byte);

    /* Run in a loop to continuously send and receive datagrams */
    int iteration = 0;
    while (1)
    {
        iteration++;
        printf("--- Iteration %d ---\n", iteration);

        /* Create a payload with iteration data */
        uint8_t payload[10];
        payload[0] = 'I';
        payload[1] = 'T';
        payload[2] = ' ';
        payload[3] = (iteration / 1000) + '0';  // Thousands digit
        payload[4] = ((iteration % 1000) / 100) + '0';  // Hundreds digit
        payload[5] = ((iteration % 100) / 10) + '0';    // Tens digit
        payload[6] = (iteration % 10) + '0';            // Ones digit
        payload[7] = '\r';
        payload[8] = '\n';
        payload[9] = 0;  // Null terminator for string operations
        
        uint8_t payload_len = 9;  // Length without null terminator

        /* Clear buffers for this iteration */
        tx_len = 0;
        rx_len = 0;
        rx_pos = 0;
        
        printf("TX: Sending datagram to addr=0x21, type=READ(0), payload=\"");
        for (int i = 0; i < payload_len; i++)
        {
            printf("%c", payload[i]);
        }
        printf("\" (hex: ");
        for (int i = 0; i < payload_len; i++)
        {
            printf("%02x ", payload[i]);
        }
        printf(")\n");

        /* Send a datagram: type READ (0x00), address 0x21, broadcast 0 (unicast) */
        proto_result_t result = proto_send_datagram(0x21, 0, 0, payload, payload_len);
        if (result != PROTO_RESULT_OK)
        {
            printf("TX: Failed to send datagram: %d\n", result);
        }
        else
        {
            printf("TX: Datagram sent successfully\n");
        }

        /* Poll several times to allow transmission and reception */
        for (int poll_count = 0; poll_count < 20; poll_count++)
        {
            proto_poll();
        }

        /* Try to receive a datagram */
        uint8_t addr, broadcast, type;
        uint8_t rx_payload[255];
        uint8_t len;
        result = proto_recv_datagram(&addr, &broadcast, &type, rx_payload, &len);
        
        if (result == PROTO_RESULT_OK)
        {
            printf("RX: Received datagram: addr=0x%02x, broadcast=%d, type=%d, len=%d\n", 
                   addr, broadcast, type, len);
            printf("RX: Payload (ASCII): \"");
            for (int i = 0; i < len; i++)
            {
                // Only print printable characters, otherwise show hex
                if (rx_payload[i] >= 32 && rx_payload[i] <= 126)
                {
                    printf("%c", rx_payload[i]);
                }
                else
                {
                    printf("<%02x>", rx_payload[i]);
                }
            }
            printf("\" (hex: ");
            for (int i = 0; i < len; i++)
            {
                printf("%02x ", rx_payload[i]);
            }
            printf(")\n");
            
            // Check if we got back what we sent
            if (len == payload_len && memcmp(rx_payload, payload, payload_len) == 0)
            {
                printf("RX: Payload matches transmitted data! ✓\n");
            }
            else
            {
                printf("RX: Payload differs from transmitted data! ✗\n");
                printf("         Expected: ");
                for (int i = 0; i < payload_len; i++)
                {
                    printf("%02x ", payload[i]);
                }
                printf("\n");
                printf("         Got:      ");
                for (int i = 0; i < len; i++)
                {
                    printf("%02x ", rx_payload[i]);
                }
                printf("\n");
            }
        }
        else
        {
            printf("RX: No datagram received: %d\n", result);
            // Debug: show what's in our buffers
            printf("         Buffer state: tx_len=%zu, rx_len=%zu, rx_pos=%zu\n", tx_len, rx_len, rx_pos);
            if (rx_len > 0)
            {
                printf("         RX buffer contents: ");
                for (size_t i = 0; i < rx_len; i++)
                {
                    printf("%02x ", rx_buffer[i]);
                }
                printf("\n");
            }
        }

        printf("\n");
        
        /* Small delay to make output readable */
        usleep(500000);  // 500ms delay
        
        /* Exit after 10 iterations for demo purposes */
        if (iteration >= 10)
        {
            printf("=== Demo completed after %d iterations ===\n", iteration);
            break;
        }
    }

    return 0;
}