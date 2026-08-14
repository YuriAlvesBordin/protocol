#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/**
 * @brief Result codes for protocol operations
 */
typedef enum {
    PROTO_RESULT_OK = 0,
    PROTO_RESULT_ERROR_TIMEOUT,
    PROTO_RESULT_ERROR_CRC,
    PROTO_RESULT_ERROR_INVALID,
    PROTO_RESULT_ERROR_STATE
} proto_result_t;

/**
 * @brief Initialize the protocol layer
 * 
 * @param tx_byte Callback to transmit a single byte
 * @param rx_byte Callback to receive a single byte (blocking or non-blocking as needed)
 */
void proto_init(void (*tx_byte)(uint8_t), uint8_t (*rx_byte)(void));

/**
 * @brief Send a datagram frame
 * 
 * @param addr Address byte (0-255)
 * @param broadcast 0 for unicast, 1 for broadcast
 * @param type Message type (0-5 for READ, WRITE, CONN_REQ, CONN_ACK, STREAM_ACK, CLOSE)
 * @param payload Pointer to payload data
 * @param len Length of payload (1-255)
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_send_datagram(uint8_t addr, uint8_t broadcast, uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Receive a datagram frame (non-blocking, checks for incoming frame)
 * 
 * @param addr Pointer to store address byte
 * @param broadcast Pointer to store broadcast flag
 * @param type Pointer to store message type
 * @param payload Pointer to buffer for payload data
 * @param len Pointer to store length of payload (max 255)
 * @return proto_result_t PROTO_RESULT_OK if a frame was received, otherwise error
 */
proto_result_t proto_recv_datagram(uint8_t *addr, uint8_t *broadcast, uint8_t *type, uint8_t *payload, uint8_t *len);

/**
 * @brief Start streaming as transmitter
 * 
 * @param transmitter_id ID of this node (0-255)
 * @param session_id Session ID for this stream (0-255)
 * @param receiver_list Array of receiver IDs
 * @param receiver_count Number of receivers in the list
 * @param initial_ack_slot Initial ACK slot suggestion (0-255)
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_tx_start(uint8_t transmitter_id, uint8_t session_id, const uint8_t *receiver_list, uint8_t receiver_count, uint8_t initial_ack_slot);

/**
 * @brief Start streaming as receiver
 * 
 * @param receiver_id ID of this node (0-255)
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_rx_start(uint8_t receiver_id);

/**
 * @brief Send a data block in streaming mode (transmitter)
 * 
 * @param data Pointer to 254 bytes of data
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_tx_send_block(const uint8_t *data);

/**
 * @brief Receive a data block in streaming mode (receiver)
 * 
 * @param data Pointer to buffer for 254 bytes of data
 * @return proto_result_t PROTO_RESULT_OK if a block was received, otherwise error
 */
proto_result_t proto_stream_rx_get_block(uint8_t *data);

/**
 * @brief Close the streaming session (transmitter)
 * 
 * @param reason Reason for closing (see CLOSE reason codes)
 */
void proto_stream_tx_close(uint8_t reason);

/**
 * @brief Close the streaming session (receiver)
 */
void proto_stream_rx_close(void);

/**
 * @brief Call periodically to handle timeouts and run state machines
 */
void proto_poll(void);

#endif // PROTOCOL_H