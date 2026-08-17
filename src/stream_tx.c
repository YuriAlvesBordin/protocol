#include "stream_tx.h"
#include "frame.h"
#include "crc16.h"
#include <string.h>
#include <stdio.h>
#include "protocol_config.h"


static uint8_t build_control(uint8_t type, uint8_t broadcast)
{
    return 0xC1 | ((type & 0x07) << 3) | ((broadcast & 0x01) << 2);
}

void stream_tx_init(stream_tx_t *ctx, void (*tx_byte)(uint8_t))
{
    ctx->state = STREAM_TX_STATE_IDLE;
    ctx->tx_byte = tx_byte;
    ctx->timeout_counter = 0;
    ctx->current_block = 0;
    ctx->retry_count = 0;
    ctx->current_receiver_index = 0;
    ctx->receiver_count = 0;
    memset(ctx->receiver_list, 0, sizeof(ctx->receiver_list));
    memset(ctx->ack_received, 0, sizeof(ctx->ack_received));
    ctx->transmitter_id = 0;
    ctx->session_id = 0;
}

proto_result_t stream_tx_start(stream_tx_t *ctx, uint8_t transmitter_id, uint8_t session_id, const uint8_t *receiver_list, uint8_t receiver_count, uint8_t initial_ack_slot)
{
    if (ctx->state != STREAM_TX_STATE_IDLE)
    {
        return PROTO_RESULT_ERROR_STATE;
    }
    if (receiver_count == 0 || receiver_count > STREAM_TX_MAX_RECEIVERS)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }

    ctx->transmitter_id = transmitter_id;
    ctx->session_id = session_id;
    ctx->receiver_count = receiver_count;
    memcpy(ctx->receiver_list, receiver_list, receiver_count);
    
    memset(ctx->ack_received, 0, receiver_count);
    ctx->current_block = 0;
    ctx->retry_count = 0;
    ctx->current_receiver_index = 0;
    ctx->state = STREAM_TX_STATE_SENDING_CONN_REQ;
    ctx->timeout_counter = STREAM_TX_TIMEOUT_CONN;

    return PROTO_RESULT_OK;
}

void stream_tx_handle_byte(stream_tx_t *ctx, uint8_t byte)
{
    
    
    
    

    
    static frame_parser_t parser;
    static int parser_initialized = 0;
    if (!parser_initialized)
    {
        frame_parser_init(&parser);
        parser_initialized = 1;
    }

    uint8_t out_control, out_address, out_payload[255];
    uint8_t out_payload_len;
    frame_result_t res = frame_parser_parse_byte(&parser, byte, &out_control, &out_address, out_payload, &out_payload_len);

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

    
    
    if (out_address != ctx->transmitter_id)
    {
        return;
    }

    switch (type)
    {
        case 3: 
            if (ctx->state == STREAM_TX_STATE_SENDING_CONN_REQ)
            {
                
                if (out_payload_len != 3)
                {
                    break;
                }
                
                uint8_t receiver_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                

                
                int found = 0;
                for (int i = 0; i < ctx->receiver_count; i++)
                {
                    if (ctx->receiver_list[i] == receiver_id)
                    {
                        found = 1;
                        printf("DEBUG: CONN_ACK from receiver_id=0x%02X\n", receiver_id);
                        
                        if (session_id == ctx->session_id)
                        {
                            printf("DEBUG: Session ID matches\n");
                            
                            
                            
                            ctx->ack_received[i] = 1; 
                        }
                        else
                        {
                            printf("DEBUG: Session ID mismatch: expected 0x%02X, got 0x%02X\n", ctx->session_id, session_id);
                        }
                        break;
                    }
                }
                if (!found)
                {
                    printf("DEBUG: CONN_ACK from unknown receiver_id=0x%02X\n", receiver_id);
                }
                
                
                
                
                
                ctx->current_receiver_index++;
                printf("DEBUG: current_receiver_index=%d, receiver_count=%d\n", ctx->current_receiver_index, ctx->receiver_count);
                if (ctx->current_receiver_index >= ctx->receiver_count)
                {
                    
                    
                    int connected_count = 0;
                    for (int i = 0; i < ctx->receiver_count; i++)
                    {
                        if (ctx->ack_received[i])
                        {
                            connected_count++;
                        }
                    }
                    printf("DEBUG: connected_count=%d\n", connected_count);
                    if (connected_count > 0)
                    {
                        
                        ctx->state = STREAM_TX_STATE_STREAM_SENDING;
                        ctx->timeout_counter = 0; 
                        printf("DEBUG: Transmitter moved to STREAM_SENDING state\n");
                    }
                    else
                    {
                        
                        ctx->state = STREAM_TX_STATE_IDLE;
                        printf("DEBUG: No receivers connected, going to IDLE\n");
                    }
                }
                else
                {
                    
                    ctx->timeout_counter = STREAM_TX_TIMEOUT_CONN;
                    printf("DEBUG: Still have receivers to try, setting timeout for next CONN_REQ\n");
                }
            }
            break;
        case 4: 
            if (ctx->state == STREAM_TX_STATE_WAIT_ACKS)
            {
                if (out_payload_len != 3)
                {
                    break;
                }
                uint8_t receiver_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                uint8_t block_number = out_payload[2];

                
                if (session_id != ctx->session_id || block_number != ctx->current_block)
                {
                    break;
                }

                
                int found = -1;
                for (int i = 0; i < ctx->receiver_count; i++)
                {
                    if (ctx->receiver_list[i] == receiver_id)
                    {
                        found = i;
                        break;
                    }
                }
                if (found == -1)
                {
                    break;
                }

                
                ctx->ack_received[found] = 1;
            }
            break;
        case 5: 
            if (ctx->state == STREAM_TX_STATE_CLOSING)
            {
                if (out_payload_len != 3)
                {
                    break;
                }
                uint8_t transmitter_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                uint8_t reason = out_payload[2];

                if (transmitter_id == ctx->transmitter_id && session_id == ctx->session_id)
                {
                    
                    ctx->state = STREAM_TX_STATE_IDLE;
                }
            }
            break;
        default:
            break;
    }
}

void stream_tx_poll(stream_tx_t *ctx)
{
    if (ctx->tx_byte == NULL)
    {
        return;
    }

    switch (ctx->state)
    {
        case STREAM_TX_STATE_SENDING_CONN_REQ:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                
                ctx->current_receiver_index++;
                if (ctx->current_receiver_index >= ctx->receiver_count)
                {
                    
                    int connected_count = 0;
                    for (int i = 0; i < ctx->receiver_count; i++)
                    {
                        if (ctx->ack_received[i])
                        {
                            connected_count++;
                        }
                    }
                    if (connected_count > 0)
                    {
                        ctx->state = STREAM_TX_STATE_STREAM_SENDING;
                        
                        printf("DEBUG: Transmitter moved to STREAM_SENDING state via timeout\n");
                    }
                    else
                    {
                        
                        ctx->state = STREAM_TX_STATE_IDLE;
                    }
                }
                else
                {
                    
                    ctx->timeout_counter = STREAM_TX_TIMEOUT_CONN;
                }
            }
            
            if (ctx->state == STREAM_TX_STATE_SENDING_CONN_REQ && ctx->current_receiver_index < ctx->receiver_count)
            {
                uint8_t receiver_id = ctx->receiver_list[ctx->current_receiver_index];
                
                uint8_t payload[3] = { ctx->transmitter_id, ctx->session_id, 0 };
                uint8_t frame_buffer[255];
                uint8_t frame_len;
                
                uint8_t control = build_control(2, 0);
                frame_build(control, receiver_id, payload, 3, frame_buffer, &frame_len);
                
                for (int i = 0; i < frame_len; i++)
                {
                    ctx->tx_byte(frame_buffer[i]);
                }
            }
            break;

        case STREAM_TX_STATE_STREAM_SENDING:
            
            
            
            
            
            
            break;

        case STREAM_TX_STATE_WAIT_ACKS:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                
                
                int ack_count = 0;
                for (int i = 0; i < ctx->receiver_count; i++)
                {
                    if (ctx->ack_received[i])
                    {
                        ack_count++;
                    }
                }
                if (ack_count == ctx->receiver_count)
                {
                    
                    ctx->current_block++;
                    ctx->retry_count = 0;
                    
                    memset(ctx->ack_received, 0, ctx->receiver_count);
                    ctx->state = STREAM_TX_STATE_STREAM_SENDING;
                }
                else
                {
                    
                    if (ctx->retry_count < STREAM_TX_MAX_RETRIES)
                    {
                        ctx->retry_count++;
                        ctx->timeout_counter = STREAM_TX_TIMEOUT_ACK;
                        
                        
                        
                        
                        
                        
                        
                    }
                    else
                    {
                        
                        ctx->state = STREAM_TX_STATE_CLOSING;
                        ctx->timeout_counter = 0; 
                    }
                }
            }
            break;

        case STREAM_TX_STATE_CLOSING:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                
                
                
                
                if (ctx->receiver_count > 0)
                {
                    uint8_t receiver_id = ctx->receiver_list[0];
                    uint8_t payload[3] = { ctx->transmitter_id, ctx->session_id, 0 }; 
                    uint8_t frame_buffer[255];
                    uint8_t frame_len;
                    uint8_t control = build_control(5, 0); 
                    frame_build(control, receiver_id, payload, 3, frame_buffer, &frame_len);
                    for (int i = 0; i < frame_len; i++)
                    {
                        ctx->tx_byte(frame_buffer[i]);
                    }
                }
                ctx->state = STREAM_TX_STATE_IDLE;
            }
            break;

        default:
            break;
    }
}

proto_result_t stream_tx_send_block(stream_tx_t *ctx, const uint8_t *data)
{
    if (ctx->state != STREAM_TX_STATE_STREAM_SENDING)
    {
        return PROTO_RESULT_ERROR_STATE;
    }
    if (data == NULL)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }

    
    
    uint8_t frame_buffer[257]; 
    uint8_t *ptr = frame_buffer;
    *ptr++ = ctx->current_block;
    memcpy(ptr, data, 254);
    ptr += 254;

    
    uint8_t crc_buffer[255]; 
    crc_buffer[0] = ctx->current_block;
    memcpy(&crc_buffer[1], data, 254);
    uint16_t crc = crc16_ccitt_false(crc_buffer, 255);
    *ptr++ = (uint8_t)(crc & 0x00FF);
    *ptr++ = (uint8_t)((crc & 0xFF00) >> 8);

    
    for (int i = 0; i < 257; i++)
    {
        ctx->tx_byte(frame_buffer[i]);
    }

    
    ctx->state = STREAM_TX_STATE_WAIT_ACKS;
    ctx->timeout_counter = STREAM_TX_TIMEOUT_ACK;
    
    memset(ctx->ack_received, 0, ctx->receiver_count);

    return PROTO_RESULT_OK;
}

void stream_tx_close(stream_tx_t *ctx, uint8_t reason)
{
    if (ctx->state == STREAM_TX_STATE_STREAM_SENDING || ctx->state == STREAM_TX_STATE_WAIT_ACKS)
    {
        ctx->state = STREAM_TX_STATE_CLOSING;
        ctx->timeout_counter = 0; 
    }
    else if (ctx->state == STREAM_TX_STATE_IDLE)
    {
        
    }
    
    else
    {
        ctx->state = STREAM_TX_STATE_IDLE;
    }
}