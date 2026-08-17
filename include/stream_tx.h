#ifndef STREAM_TX_H
#define STREAM_TX_H

#include <stdint.h>
#include "proto_result.h"
#include "protocol_config.h"

/**
 * @brief Streaming transmitter states
 */
typedef enum {
    STREAM_TX_STATE_IDLE = 0,
    STREAM_TX_STATE_SENDING_CONN_REQ,
    STREAM_TX_STATE_STREAM_SENDING,
    STREAM_TX_STATE_WAIT_ACKS,
    STREAM_TX_STATE_CLOSING
} stream_tx_state_t;

/**
 * @brief Streaming transmitter context
 */
typedef struct {
    stream_tx_state_t state;
    uint8_t transmitter_id;
    uint8_t session_id;
    uint8_t receiver_list[STREAM_TX_MAX_RECEIVERS];
    uint8_t receiver_count;
    uint8_t current_block;
    uint8_t retry_count;
    uint8_t ack_received[STREAM_TX_MAX_RECEIVERS]; 
    uint8_t current_receiver_index; 
    uint16_t timeout_counter;
    void (*tx_byte)(uint8_t); 
} stream_tx_t;

/**
 * @brief Initialize streaming transmitter context
 * 
 * @param ctx Pointer to context
 * @param tx_byte Callback to transmit a single byte
 */
void stream_tx_init(stream_tx_t *ctx, void (*tx_byte)(uint8_t));

/**
 * @brief Start streaming as transmitter
 * 
 * @param ctx Pointer to context
 * @param transmitter_id ID of this node
 * @param session_id Session ID for this stream
 * @param receiver_list Array of receiver IDs
 * @param receiver_count Number of receivers in the list
 * @param initial_ack_slot Initial ACK slot suggestion (unused in this implementation, kept for compatibility)
 * @return proto_result_t Status of the operation
 */
proto_result_t stream_tx_start(stream_tx_t *ctx, uint8_t transmitter_id, uint8_t session_id, const uint8_t *receiver_list, uint8_t receiver_count, uint8_t initial_ack_slot);

/**
 * @brief Handle an incoming byte for the streaming transmitter
 * 
 * @param ctx Pointer to context
 * @param byte Input byte
 */
void stream_tx_handle_byte(stream_tx_t *ctx, uint8_t byte);

/**
 * @brief Poll the streaming transmitter (call periodically to handle timeouts)
 * 
 * @param ctx Pointer to context
 */
void stream_tx_poll(stream_tx_t *ctx);

/**
 * @brief Send a data block (called by the application when ready to send next block)
 * 
 * @param ctx Pointer to context
 * @param data Pointer to 254 bytes of data
 * @return proto_result_t PROTO_RESULT_OK if block was accepted for transmission, otherwise error
 */
proto_result_t stream_tx_send_block(stream_tx_t *ctx, const uint8_t *data);

/**
 * @brief Close the streaming session
 * 
 * @param ctx Pointer to context
 * @param reason Reason for closing
 */
void stream_tx_close(stream_tx_t *ctx, uint8_t reason);

#endif 