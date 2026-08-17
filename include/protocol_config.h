#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

/**
 * @brief Configuration options for the protocol stack
 * 
 * These values can be overridden by defining them before including this header.
 */

/** Frame related configurations */
#ifndef PROTO_MAX_PAYLOAD_LENGTH
#define PROTO_MAX_PAYLOAD_LENGTH      255u    /**< Maximum payload length in bytes */
#endif

#ifndef PROTO_FRAME_BUFFER_SIZE
#define PROTO_FRAME_BUFFER_SIZE       (PROTO_MAX_PAYLOAD_LENGTH + 5)  /**< Maximum frame size (header+length+payload+CRC) */
#endif

#ifndef PROTO_CRC_BUFFER_SIZE
#define PROTO_CRC_BUFFER_SIZE         (PROTO_MAX_PAYLOAD_LENGTH + 3)  /**< Buffer size for CRC calculation (header+payload) */
#endif

/** Streaming Transmitter configurations */
#ifndef STREAM_TX_MAX_RECEIVERS
#define STREAM_TX_MAX_RECEIVERS       255u    /**< Maximum number of receivers in a streaming session */
#endif

#ifndef STREAM_TX_TIMEOUT_CONN
#define STREAM_TX_TIMEOUT_CONN        100u    /**< Connection request timeout (poll cycles) */
#endif

#ifndef STREAM_TX_TIMEOUT_ACK
#define STREAM_TX_TIMEOUT_ACK         50u     /**< ACK timeout for streaming blocks (poll cycles) */
#endif

#ifndef STREAM_TX_MAX_RETRIES
#define STREAM_TX_MAX_RETRIES         3u      /**< Maximum retransmission attempts for a block */
#endif

/** Streaming Receiver configurations */
#ifndef STREAM_RX_TIMEOUT_ACK_TURN
#define STREAM_RX_TIMEOUT_ACK_TURN    50u     /**< ACK turn timeout (poll cycles) */
#endif

#ifndef STREAM_RX_TIMEOUT_WAITING
#define STREAM_RX_TIMEOUT_WAITING     1000u   /**< Waiting state timeout (poll cycles) */
#endif

/** Example configurations (can be overridden) */
#ifndef EXAMPLE_TX_BUFFER_SIZE
#define EXAMPLE_TX_BUFFER_SIZE        256u    /**< TX buffer size in example program */
#endif

#endif /** PROTOCOL_CONFIG_H */