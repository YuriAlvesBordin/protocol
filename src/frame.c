#include "frame.h"
#include "crc16.h"
#include "protocol_config.h"

#define FRAME_SYNC_MASK          0xC0
#define FRAME_SYNC_VALUE         0xC0
#define FRAME_SYNC_EXT_MASK      0x03
#define FRAME_SYNC_EXT_VALUE     0x01
#define FRAME_TYPE_SHIFT         3
#define FRAME_TYPE_MASK          0x38
#define FRAME_BROADCAST_SHIFT    2
#define FRAME_BROADCAST_MASK     0x04

void frame_parser_init(frame_parser_t *parser)
{
    parser->state = FRAME_STATE_SEEK_SYNC;
    parser->payload_index = 0;
    parser->crc_byte_index = 0;
}

frame_result_t frame_parser_parse_byte(frame_parser_t *parser, uint8_t byte, uint8_t *out_control, uint8_t *out_address, uint8_t *out_payload, uint8_t *out_payload_len)
{
    frame_result_t result = FRAME_RESULT_INCOMPLETE;

    switch (parser->state)
    {
        case FRAME_STATE_SEEK_SYNC:
            
            if ((byte & FRAME_SYNC_MASK) == FRAME_SYNC_VALUE && (byte & FRAME_SYNC_EXT_MASK) == FRAME_SYNC_EXT_VALUE)
            {
                parser->control = byte;
                parser->state = FRAME_STATE_READ_ADDRESS;
            }
            else
            {
                result = FRAME_RESULT_ERROR_SYNC;
            }
            break;

        case FRAME_STATE_READ_ADDRESS:
            parser->address = byte;
            parser->state = FRAME_STATE_READ_LENGTH;
            break;

        case FRAME_STATE_READ_LENGTH:
            parser->length = byte;
            if (parser->length == 0)
            {
                parser->state = FRAME_STATE_SEEK_SYNC;
                result = FRAME_RESULT_ERROR_LENGTH;
            }
            else if (parser->length > PROTO_MAX_PAYLOAD_LENGTH)
            {
                
                parser->state = FRAME_STATE_SEEK_SYNC;
                result = FRAME_RESULT_ERROR_LENGTH;
            }
            else
            {
                parser->payload_index = 0;
                parser->state = FRAME_STATE_READ_BODY;
            }
            break;

        case FRAME_STATE_READ_BODY:
            if (parser->payload_index < parser->length)
            {
                parser->payload[parser->payload_index] = byte;
                parser->payload_index++;
            }
            if (parser->payload_index == parser->length)
            {
                parser->state = FRAME_STATE_VALIDATE_CRC;
                parser->crc_byte_index = 0;
            }
            break;

        case FRAME_STATE_VALIDATE_CRC:
            
            parser->crc_bytes[parser->crc_byte_index] = byte;
            parser->crc_byte_index++;

            if (parser->crc_byte_index == 2)
            {
                
                uint16_t received_crc = (uint16_t)parser->crc_bytes[0] | ((uint16_t)parser->crc_bytes[1] << 8);

                
                uint8_t crc_buffer[PROTO_CRC_BUFFER_SIZE]; 
                uint8_t *crc_ptr = crc_buffer;
                *crc_ptr++ = parser->control;
                *crc_ptr++ = parser->address;
                *crc_ptr++ = parser->length;
                for (int i = 0; i < parser->payload_index; i++)
                {
                    *crc_ptr++ = parser->payload[i];
                }
                uint16_t computed_crc = crc16_ccitt_false(crc_buffer, (uint16_t)(crc_ptr - crc_buffer));

                if (computed_crc == received_crc)
                {
                    
                    *out_control = parser->control;
                    *out_address = parser->address;
                    for (int i = 0; i < parser->payload_index; i++)
                    {
                        out_payload[i] = parser->payload[i];
                    }
                    *out_payload_len = parser->payload_index;
                    parser->state = FRAME_STATE_SEEK_SYNC;
                    parser->crc_byte_index = 0;
                    result = FRAME_RESULT_OK;
                }
                else
                {
                    parser->state = FRAME_STATE_SEEK_SYNC;
                    parser->crc_byte_index = 0;
                    result = FRAME_RESULT_ERROR_CRC;
                }
            }
            else
            {
                
                result = FRAME_RESULT_INCOMPLETE;
            }
            break;
    }

    return result;
}

void frame_build(uint8_t control, uint8_t address, const uint8_t *payload, uint8_t len, uint8_t *out_buffer, uint8_t *out_length)
{
    
    if (len == 0 || len > PROTO_MAX_PAYLOAD_LENGTH)
    {
        *out_length = 0;
        return;
    }

    
    uint8_t *ptr = out_buffer;
    *ptr++ = control;
    *ptr++ = address;
    *ptr++ = len;
    for (int i = 0; i < len; i++)
    {
        *ptr++ = payload[i];
    }

    
    uint8_t crc_buffer[PROTO_CRC_BUFFER_SIZE]; 
    uint8_t *crc_ptr = crc_buffer;
    *crc_ptr++ = control;
    *crc_ptr++ = address;
    *crc_ptr++ = len;
    for (int i = 0; i < len; i++)
    {
        *crc_ptr++ = payload[i];
    }
    uint16_t crc = crc16_ccitt_false(crc_buffer, (uint16_t)(crc_ptr - crc_buffer));

    
    *ptr++ = (uint8_t)(crc & 0x00FF);
    *ptr++ = (uint8_t)((crc & 0xFF00) >> 8);

    *out_length = (uint8_t)(ptr - out_buffer);
}