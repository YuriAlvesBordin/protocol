#ifndef STREAM_RX_H
#define STREAM_RX_H

#include <stdint.h>
#include "frame.h"

/**
 * @brief Streaming receiver states
 */
typedef enum {
    STREAM_RX_STATE_IDLE = 0,
    STREAM_RX_STATE_STREAM_RECEIVING,
    STREAM_RX_STATE_WAITING
} stream_rx_state_t;

/**
 * @brief Streaming receiver context
 */
typedef struct {
    stream_rx_state_t state;
    uint8_t receiver_id;
    uint8_t transmitter_id; // Valid in STREAM_RECEIVING and WAITING
    uint8_t session_id;     // Valid in STREAM_RECEIVING and WAITING
    uint8_t current_block;  // Expected block number in STREAM_RECEIVING
    uint8_t last_block_received; // Last block we successfully received and delivered
    uint8_t predecessor_address; // (receiver_id - 1) & 0xFF
    uint8_t ack_to_send; // 1 if we have received a block and are ready to send ACK (waiting for predecessor or timeout)
    uint8_t ack_sent_for_last_block; // 1 if we have already sent an ACK for the last_block_received
    uint16_t timeout_counter;
    void (*tx_byte)(uint8_t);
    uint8_t received_data[254];
    uint8_t data_available;
    frame_parser_t parser; // For parsing incoming normal frames
} stream_rx_t;

/**
 * @brief Initialize streaming receiver context
 * 
 * @param ctx Pointer to context
 * @param tx_byte Callback to transmit a single byte
 */
void stream_rx_init(stream_rx_t *ctx, void (*tx_byte)(uint8_t));

/**
 * @brief Start streaming as receiver
 * 
 * @param ctx Pointer to context
 * @param receiver_id ID of this node
 * @return proto_result_t Status of the operation
 */
proto_result_t stream_rx_start(stream_rx_t *ctx, uint8_t receiver_id);

/**
 * @brief Handle an incoming byte for the streaming receiver
 * 
 * @param ctx Pointer to context
 * @param byte Input byte
 */
void stream_rx_handle_byte(stream_rx_t *ctx, uint8_t byte);

/**
 * @brief Poll the streaming receiver (call periodically to handle timeouts)
 * 
 * @param ctx Pointer to context
 */
void stream_rx_poll(stream_rx_t *ctx);

/**
 * @brief Get a received data block (call when application is ready to receive next block)
 * 
 * @param ctx Pointer to context
 * @param data Pointer to buffer for 254 bytes of data
 * @return proto_result_t PROTO_RESULT_OK if a block was available, otherwise error
 */
proto_result_t stream_rx_get_block(stream_rx_t *ctx, uint8_t *data);

/**
 * @brief Close the streaming session (receiver)
 * 
 * @param ctx Pointer to context
 */
void stream_rx_close(stream_rx_t *ctx);

#endif // STREAM_RX_H