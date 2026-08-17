#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "frame.h"
#include "stream_tx.h"
#include "stream_rx.h"
#include "proto_result.h"

/**
 * @brief Protocol context
 */
typedef struct {
    void (*tx_byte)(uint8_t);
    uint8_t (*rx_byte)(void);
    uint8_t frame_control;
    uint8_t frame_address;
    uint8_t frame_payload[255];
    uint8_t frame_payload_len;
    uint8_t frame_valid;
    frame_parser_t frame_parser;
    stream_tx_t stream_tx;
    stream_rx_t stream_rx;
} protocol_t;

/**
 * @brief Initialize the protocol layer
 * 
 * @param ctx Pointer to protocol context
 * @param tx_byte Callback to transmit a single byte
 * @param rx_byte Callback to receive a single byte (blocking or non-blocking as needed)
 */
void proto_init(protocol_t* ctx, void (*tx_byte)(uint8_t), uint8_t (*rx_byte)(void));

/**
 * @brief Send a datagram frame
 * 
 * @param ctx Pointer to protocol context
 * @param addr Address byte (0-255)
 * @param broadcast 0 for unicast, 1 for broadcast
 * @param type Message type (0-5 for READ, WRITE, CONN_REQ, CONN_ACK, STREAM_ACK, CLOSE)
 * @param len Length of payload (1-255)
 * @param payload Pointer to payload data
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_send_datagram(protocol_t* ctx, uint8_t addr, uint8_t broadcast, uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Receive a datagram frame (non-blocking, checks for incoming frame)
 * 
 * @param ctx Pointer to protocol context
 * @param addr Pointer to store address byte
 * @param broadcast Pointer to store broadcast flag
 * @param type Pointer to store message type
 * @param len Pointer to store length of payload (max 255)
 * @return proto_result_t PROTO_RESULT_OK if a frame was received, otherwise error
 */
proto_result_t proto_recv_datagram(protocol_t* ctx, uint8_t *addr, uint8_t *broadcast, uint8_t *type, uint8_t *payload, uint8_t *len);

/**
 * @brief Start streaming as transmitter
 * 
 * @param ctx Pointer to protocol context
 * @param transmitter_id ID of this node (0-255)
 * @param session_id Session ID for this stream (0-255)
 * @param receiver_list Array of receiver IDs
 * @param receiver_count Number of receivers in the list
 * @param initial_ack_slot Initial ACK slot suggestion (0-255)
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_tx_start(protocol_t* ctx, uint8_t transmitter_id, uint8_t session_id, const uint8_t *receiver_list, uint8_t receiver_count, uint8_t initial_ack_slot);

/**
 * @brief Send a data block in streaming mode (transmitter)
 * 
 * @param ctx Pointer to protocol context
 * @param data Pointer to 254 bytes of data
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_tx_send_block(protocol_t* ctx, const uint8_t *data);

/**
 * @brief Close the streaming session (transmitter)
 * 
 * @param ctx Pointer to protocol context
 * @param reason Reason for closing (see CLOSE reason codes)
 */
void proto_stream_tx_close(protocol_t* ctx, uint8_t reason);

/**
 * @brief Start streaming as receiver
 * 
 * @param ctx Pointer to protocol context
 * @param receiver_id ID of this node (0-255)
 * @return proto_result_t Status of the operation
 */
proto_result_t proto_stream_rx_start(protocol_t* ctx, uint8_t receiver_id);

/**
 * @brief Receive a data block in streaming mode (receiver)
 * 
 * @param ctx Pointer to protocol context
 * @param data Pointer to buffer for 254 bytes of data
 * @return proto_result_t PROTO_RESULT_OK if a block was received, otherwise error
 */
proto_result_t proto_stream_rx_get_block(protocol_t* ctx, uint8_t *data);

/**
 * @brief Close the streaming session (receiver)
 * 
 * @param ctx Pointer to protocol context
 */
void proto_stream_rx_close(protocol_t* ctx);

/**
 * @brief Call periodically to handle timeouts and run state machines
 * 
 * @param ctx Pointer to protocol context
 */
void proto_poll(protocol_t* ctx);

#endif 