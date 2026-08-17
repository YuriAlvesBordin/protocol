#include "stream_rx.h"
#include <string.h>
#include "protocol_config.h"


static uint8_t build_control(uint8_t type, uint8_t broadcast)
{
    return 0xC1 | ((type & 0x07) << 3) | ((broadcast & 0x01) << 2);
}

void stream_rx_init(stream_rx_t *ctx, void (*tx_byte)(uint8_t))
{
    ctx->state = STREAM_RX_STATE_IDLE;
    ctx->tx_byte = tx_byte;
    ctx->timeout_counter = 0;
    ctx->data_available = 0;
    ctx->ack_to_send = 0;
    ctx->ack_sent_for_last_block = 0;
    ctx->last_block_received = 0xFF; 
    frame_parser_init(&ctx->parser);
}

proto_result_t stream_rx_start(stream_rx_t *ctx, uint8_t receiver_id)
{
    if (ctx->state != STREAM_RX_STATE_IDLE)
    {
        return PROTO_RESULT_ERROR_STATE;
    }

    ctx->receiver_id = receiver_id;
    ctx->predecessor_address = (receiver_id - 1) & 0xFF;
    ctx->state = STREAM_RX_STATE_IDLE; 
    ctx->timeout_counter = 0;
    ctx->data_available = 0;
    ctx->ack_to_send = 0;
    ctx->ack_sent_for_last_block = 0;
    ctx->last_block_received = 0xFF;
    frame_parser_init(&ctx->parser);

    return PROTO_RESULT_OK;
}

void stream_rx_handle_byte(stream_rx_t *ctx, uint8_t byte)
{
    uint8_t out_control, out_address, out_payload[255];
    uint8_t out_payload_len;
    frame_result_t res = frame_parser_parse_byte(&ctx->parser, byte, &out_control, &out_address, out_payload, &out_payload_len);

    if (res != FRAME_RESULT_OK)
    {
        
        return;
    }

    
    uint8_t type = (out_control & 0x38) >> 3;
    uint8_t broadcast = (out_control & 0x04) >> 2;

    
    if (broadcast != 0)
    {
        return;
    }

    
    
    uint8_t transmitter_id = out_address;

    switch (type)
    {
        case 2: 
            if (ctx->state == STREAM_RX_STATE_IDLE)
            {
                
                if (out_payload_len != 3)
                {
                    break;
                }
                
                uint8_t tx_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                

                
                if (tx_id == ctx->receiver_id)
                {
                    
                    
                    ctx->transmitter_id = tx_id;
                    ctx->session_id = session_id;
                    ctx->current_block = 0;
                    ctx->last_block_received = 0xFF;
                    ctx->ack_to_send = 0;
                    ctx->ack_sent_for_last_block = 0;
                    ctx->data_available = 0;
                    ctx->state = STREAM_RX_STATE_STREAM_RECEIVING;

                    
                    uint8_t payload[3] = { ctx->receiver_id, ctx->session_id, 0 };
                    uint8_t frame_buffer[255];
                    uint8_t frame_len;
                    uint8_t control = build_control(3, 0); 
                    frame_build(control, ctx->transmitter_id, payload, 3, frame_buffer, &frame_len);
                    for (int i = 0; i < frame_len; i++)
                    {
                        ctx->tx_byte(frame_buffer[i]);
                    }
                }
                else
                {
                    
                    ctx->transmitter_id = tx_id;
                    ctx->session_id = session_id;
                    ctx->state = STREAM_RX_STATE_WAITING;
                    ctx->timeout_counter = STREAM_RX_TIMEOUT_WAITING;
                }
            }
            break;

        case 4: 
            
            
            
            
            if (out_payload_len != 3)
            {
                break;
            }
            uint8_t rx_id = out_payload[0];
            uint8_t rx_session_id = out_payload[1];
            uint8_t rx_block_number = out_payload[2];

            
            
            if (ctx->state == STREAM_RX_STATE_STREAM_RECEIVING || ctx->state == STREAM_RX_STATE_WAITING)
            {
                if (rx_session_id == ctx->session_id)
                {
                    
                    if (ctx->state == STREAM_RX_STATE_WAITING)
                    {
                        ctx->timeout_counter = STREAM_RX_TIMEOUT_WAITING;
                    }
                    
                    
                    
                }
            }
            break;

        case 5: 
            if (ctx->state == STREAM_RX_STATE_STREAM_RECEIVING || ctx->state == STREAM_RX_STATE_WAITING)
            {
                if (out_payload_len != 3)
                {
                    break;
                }
                uint8_t tx_id = out_payload[0];
                uint8_t rx_session_id = out_payload[1];
                uint8_t reason = out_payload[2];

                if (tx_id == ctx->transmitter_id && rx_session_id == ctx->session_id)
                {
                    
                    ctx->state = STREAM_RX_STATE_IDLE;
                }
            }
            break;

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        default:
            break;
    }
}

void stream_rx_poll(stream_rx_t *ctx)
{
    if (ctx->tx_byte == NULL)
    {
        return;
    }

    switch (ctx->state)
    {
        case STREAM_RX_STATE_STREAM_RECEIVING:
            
            
            
            
            
            
            
            break;

        case STREAM_RX_STATE_WAITING:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                
                ctx->state = STREAM_RX_STATE_IDLE;
            }
            break;

        default:
            break;
    }
}

proto_result_t stream_rx_get_block(stream_rx_t *ctx, uint8_t *data)
{
    if (ctx->state != STREAM_RX_STATE_STREAM_RECEIVING)
    {
        return PROTO_RESULT_ERROR_STATE;
    }
    if (!ctx->data_available)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }
    if (data == NULL)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }
    memcpy(data, ctx->received_data, 254);
    ctx->data_available = 0;
    
    ctx->last_block_received = ctx->current_block;
    ctx->ack_sent_for_last_block = 0;
    
    ctx->current_block++;
    return PROTO_RESULT_OK;
}

void stream_rx_close(stream_rx_t *ctx)
{
    ctx->state = STREAM_RX_STATE_IDLE;
}