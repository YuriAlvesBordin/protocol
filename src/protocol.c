#include "protocol.h"
#include "frame.h"
#include "stream_tx.h"
#include "stream_rx.h"
#include <string.h>

void proto_init(protocol_t* ctx, void (*tx_byte)(uint8_t), uint8_t (*rx_byte)(void))
{
    ctx->tx_byte = tx_byte;
    ctx->rx_byte = rx_byte;
    ctx->frame_valid = 0;
    frame_parser_init(&ctx->frame_parser);
    stream_tx_init(&ctx->stream_tx, tx_byte);
    stream_rx_init(&ctx->stream_rx, tx_byte);
}

proto_result_t proto_send_datagram(protocol_t* ctx, uint8_t addr, uint8_t broadcast, uint8_t type, const uint8_t *payload, uint8_t len)
{
    if (ctx->tx_byte == NULL || payload == NULL)
    {
        return PROTO_RESULT_ERROR_STATE;
    }
    if (len == 0 || len > 255)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }
    uint8_t control = 0xC1 | ((type & 0x07) << 3) | ((broadcast & 0x01) << 2);
    uint8_t frame_buffer[255];
    uint8_t frame_len;
    frame_build(control, addr, payload, len, frame_buffer, &frame_len);
    for (int i = 0; i < frame_len; i++)
    {
        ctx->tx_byte(frame_buffer[i]);
    }
    return PROTO_RESULT_OK;
}

proto_result_t proto_recv_datagram(protocol_t* ctx, uint8_t *addr, uint8_t *broadcast, uint8_t *type, uint8_t *payload, uint8_t *len)
{
    if (!ctx->frame_valid)
    {
        return PROTO_RESULT_ERROR_INVALID;
    }
    *addr = ctx->frame_address;
    *broadcast = (ctx->frame_control & 0x04) >> 2;
    *type = (ctx->frame_control & 0x38) >> 3;
    if (payload != NULL && len != NULL)
    {
        *len = ctx->frame_payload_len;
        if (*len > 0)
        {
            memcpy(payload, ctx->frame_payload, *len);
        }
    }
    ctx->frame_valid = 0;
    return PROTO_RESULT_OK;
}

proto_result_t proto_stream_tx_start(protocol_t* ctx, uint8_t transmitter_id, uint8_t session_id, const uint8_t *receiver_list, uint8_t receiver_count, uint8_t initial_ack_slot)
{
    return stream_tx_start(&ctx->stream_tx, transmitter_id, session_id, receiver_list, receiver_count, initial_ack_slot);
}

proto_result_t proto_stream_tx_send_block(protocol_t* ctx, const uint8_t *data)
{
    return stream_tx_send_block(&ctx->stream_tx, data);
}

void proto_stream_tx_close(protocol_t* ctx, uint8_t reason)
{
    stream_tx_close(&ctx->stream_tx, reason);
}

proto_result_t proto_stream_rx_start(protocol_t* ctx, uint8_t receiver_id)
{
    return stream_rx_start(&ctx->stream_rx, receiver_id);
}

proto_result_t proto_stream_rx_get_block(protocol_t* ctx, uint8_t *data)
{
    return stream_rx_get_block(&ctx->stream_rx, data);
}

void proto_stream_rx_close(protocol_t* ctx)
{
    stream_rx_close(&ctx->stream_rx);
}

void proto_poll(protocol_t* ctx)
{
    if (ctx->rx_byte == NULL)
    {
        return;
    }
    uint8_t byte = ctx->rx_byte();
    
    
    uint8_t out_control, out_address, out_payload[255];
    uint8_t out_payload_len;
    frame_result_t frame_res = frame_parser_parse_byte(&ctx->frame_parser, byte, &out_control, &out_address, out_payload, &out_payload_len);
    if (frame_res == FRAME_RESULT_OK)
    {
        
        ctx->frame_control = out_control;
        ctx->frame_address = out_address;
        memcpy(ctx->frame_payload, out_payload, out_payload_len);
        ctx->frame_payload_len = out_payload_len;
        ctx->frame_valid = 1;
    }
    
    stream_tx_handle_byte(&ctx->stream_tx, byte);
    
    stream_rx_handle_byte(&ctx->stream_rx, byte);
    
    stream_tx_poll(&ctx->stream_tx);
    stream_rx_poll(&ctx->stream_rx);
}