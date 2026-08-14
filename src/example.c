#include <stdio.h>
#include <string.h>
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
    printf("Protocol Configuration:\n");
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

    /* Send a datagram: type READ (0x00), address 0x21, broadcast 0 (unicast), payload "Hello" */
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