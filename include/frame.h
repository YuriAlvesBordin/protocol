#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

/**
 * @brief Frame parser states
 */
typedef enum {
    FRAME_STATE_SEEK_SYNC = 0,
    FRAME_STATE_READ_ADDRESS,
    FRAME_STATE_READ_LENGTH,
    FRAME_STATE_READ_BODY,
    FRAME_STATE_VALIDATE_CRC
} frame_parser_state_t;

/**
 * @brief Frame parser result
 */
typedef enum {
    FRAME_RESULT_INCOMPLETE = 0,
    FRAME_RESULT_OK,
    FRAME_RESULT_ERROR_SYNC,
    FRAME_RESULT_ERROR_TYPE,
    FRAME_RESULT_ERROR_LENGTH,
    FRAME_RESULT_ERROR_CRC
} frame_result_t;

/**
 * @brief Frame parser context
 */
typedef struct {
    frame_parser_state_t state;
    uint8_t control;
    uint8_t address;
    uint8_t length;
    uint8_t payload[255];
    uint8_t payload_index;
    uint8_t crc_byte_index;
    uint8_t crc_bytes[2];
} frame_parser_t;

/**
 * @brief Initialize frame parser
 * 
 * @param parser Pointer to parser context
 */
void frame_parser_init(frame_parser_t *parser);

/**
 * @brief Parse a single byte and update parser state
 * 
 * @param parser Pointer to parser context
 * @param byte Input byte
 * @param out_control Pointer to store control byte (when frame complete)
 * @param out_address Pointer to store address byte (when frame complete)
 * @param out_payload Pointer to store payload data (when frame complete)
 * @param out_payload_len Pointer to store payload length (when frame complete)
 * @return frame_result_t Result of parsing
 */
frame_result_t frame_parser_parse_byte(frame_parser_t *parser, uint8_t byte, uint8_t *out_control, uint8_t *out_address, uint8_t *out_payload, uint8_t *out_payload_len);

/**
 * @brief Build a normal frame
 * 
 * @param control Control byte
 * @param address Address byte
 * @param payload Pointer to payload data
 * @param len Length of payload (1-255)
 * @param out_buffer Buffer to store the complete frame (must be at least len+5 bytes)
 * @param out_length Pointer to store the length of the generated frame
 * @return void
 */
void frame_build(uint8_t control, uint8_t address, const uint8_t *payload, uint8_t len, uint8_t *out_buffer, uint8_t *out_length);

#endif // FRAME_H