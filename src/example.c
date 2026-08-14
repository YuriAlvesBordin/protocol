#include <stdio.h>
#include <string.h>
#include "protocol.h"

/* Simple example showing how to use the protocol for datagram communication */

/* Transmission buffer for loopback test */
static uint8_t tx_buffer[256];
static size_t tx_len = 0;

/* Simple transmit function */
static void tx_byte(uint8_t b)
{
    if (tx_len < sizeof(tx_buffer))
    {
        tx_buffer[tx_len++] = b;
    }
}

/* Simple receive function */
static uint8_t rx_byte(void)
{
    static size_t rx_index = 0;
    if (rx_index < tx_len)
    {
        return tx_buffer[rx_index++];
    }
    return 0; /* Indicate no data available */
}

int main(void)
{
    /* Initialize the protocol */
    proto_init(tx_byte, rx_byte);

    /* Send a datagram: type READ (0x01), address 0x21, broadcast 0 (unicast), payload "Hello" */
    uint8_t payload[] = { 'H', 'e', 'l', 'l', 'o' };
    proto_result_t result = proto_send_datagram(0x21, 0, 0, payload, sizeof(payload));
    if (result != PROTO_RESULT_OK)
    {
        printf("Failed to send datagram: %d\n", result);
        return 1;
    }
    printf("Sent datagram\n");

    /* Poll a few times to allow transmission and reception */
    for (int i = 0; i < 10; i++)
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
        printf("Received datagram: addr=0x%02x, broadcast=%d, type=%d, len=%d\n", addr, broadcast, type, len);
        printf("Payload: ");
        for (int i = 0; i < len; i++)
        {
            printf("%02x ", rx_payload[i]);
        }
        printf("\n");
    }
    else
    {
        printf("No datagram received: %d\n", result);
    }

    return 0;
}