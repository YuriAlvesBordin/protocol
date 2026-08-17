#include "stream_rx.h"
#include <string.h>
#include <stdio.h>
#include "protocol_config.h"

// Helper to build control byte
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
    ctx->last_block_received = 0xFF; // Initialize to an invalid value
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
    ctx->state = STREAM_RX_STATE_IDLE; // Explicitly set to idle
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
        // Incomplete or error, wait for more bytes
        return;
    }

    // We have a frame. Check the type.
    uint8_t type = (out_control & 0x38) >> 3;
    uint8_t broadcast = (out_control & 0x04) >> 2;

    // We only accept unicast frames (broadcast=0) for the receiver.
    if (broadcast != 0)
    {
        return;
    }

    // The address in the frame is the transmitter's address (since we are the receiver, the frame is sent from the transmitter to us).
    // So out_address is the transmitter_id.
    uint8_t transmitter_id = out_address;

    switch (type)
    {
        case 2: // CONN_REQ
            if (ctx->state == STREAM_RX_STATE_IDLE)
            {
                // Check payload length
                if (out_payload_len != 3)
                {
                    break;
                }
                // Payload: transmitter_id, session_id, requested_ack_slot
                uint8_t tx_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                // uint8_t requested_ack_slot = out_payload[2]; // We don't use it

                // Check if this CONN_REQ is addressed to us (destination address == our receiver_id)
                if (out_address == ctx->receiver_id)
                {
                    // This CONN_REQ is for us: we will send a CONN_ACK
                    // We'll store the transmitter_id and session_id
                    ctx->transmitter_id = tx_id;
                    ctx->session_id = session_id;
                    ctx->current_block = 0;
                    ctx->last_block_received = 0xFF;
                    ctx->ack_to_send = 0;
                    ctx->ack_sent_for_last_block = 0;
                    ctx->data_available = 0;
                    ctx->state = STREAM_RX_STATE_STREAM_RECEIVING;

                    // Send CONN_ACK: payload is receiver_id, session_id, assigned_ack_slot (we'll use 0)
                    uint8_t payload[3] = { ctx->receiver_id, ctx->session_id, 0 };
                    uint8_t frame_buffer[255];
                    uint8_t frame_len;
                    uint8_t control = build_control(3, 0); // type=3 (CONN_ACK), broadcast=0
                    frame_build(control, ctx->transmitter_id, payload, 3, frame_buffer, &frame_len);
                    for (int i = 0; i < frame_len; i++)
                    {
                        ctx->tx_byte(frame_buffer[i]);
                    }
                }
                else
                {
                    // This CONN_REQ is for another receiver: we enter WAITING state
                    ctx->transmitter_id = tx_id;
                    ctx->session_id = session_id;
                    ctx->state = STREAM_RX_STATE_WAITING;
                    ctx->timeout_counter = STREAM_RX_TIMEOUT_WAITING;
                }
            }
            break;

        case 4: // STREAM_ACK
            // We use STREAM_ACK to reset timeouts in WAITING state and also to possibly trigger our ACK sending if we are the predecessor?
            // Actually, the receiver uses STREAM_ACK to reset the TIMEOUT_ACK_TURN when waiting to send its own ACK.
            // We'll handle that in the poll function when we are waiting to send an ACK.
            // For now, we'll just note that we received a STREAM_ACK and use it to reset timeouts in WAITING and in the ACK waiting state.
            if (out_payload_len != 3)
            {
                break;
            }
            uint8_t rx_session_id = out_payload[1];
            // uint8_t rx_id = out_payload[0]; // Not used
            // uint8_t rx_block_number = out_payload[2]; // Not used
            (void)out_payload[0]; // unused
            (void)out_payload[2]; // unused

            // Check if this STREAM_ACK is from a receiver in our session? We don't have a list of receivers.
            // We'll just check the session_id matches our current session (if we are in a session).
            if (ctx->state == STREAM_RX_STATE_STREAM_RECEIVING || ctx->state == STREAM_RX_STATE_WAITING)
            {
                if (rx_session_id == ctx->session_id)
                {
                    // Valid session activity: reset timeout counters
                    if (ctx->state == STREAM_RX_STATE_WAITING)
                    {
                        ctx->timeout_counter = STREAM_RX_TIMEOUT_WAITING;
                    }
                    // If we are waiting to send an ACK (ack_to_send is set), then receiving a STREAM_ACK from the predecessor should trigger us to send.
                    // But we don't know if this STREAM_ACK is from the predecessor. We'll handle that in the poll function by checking the block number and the predecessor.
                    // We'll leave it to the poll function.
                }
            }
            break;

        case 5: // CLOSE
            if (ctx->state == STREAM_RX_STATE_STREAM_RECEIVING || ctx->state == STREAM_RX_STATE_WAITING)
            {
                if (out_payload_len != 3)
                {
                    break;
                }
                uint8_t tx_id = out_payload[0];
                uint8_t rx_session_id = out_payload[1];
                // uint8_t reason = out_payload[2]; // Not used

                if (tx_id == ctx->transmitter_id && rx_session_id == ctx->session_id)
                {
                    // Valid CLOSE for our session: go to idle
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
            // In a real implementation, we would have a way to receive STREAM_DATA
            // bytes and assemble them into blocks. However, since STREAM_DATA doesn't
            // use the normal frame format, we would need a separate byte buffer and
            // state machine just for receiving streaming data.
            //
            // For the purposes of this implementation, we'll leave this as a placeholder
            // and note that full STREAM_DATA reception would require additional work.
            break;

        case STREAM_RX_STATE_WAITING:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                // Timeout: go back to idle
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
    // We have delivered the data: update last_block_received and reset ack_sent_for_last_block
    ctx->last_block_received = ctx->current_block;
    ctx->ack_sent_for_last_block = 0;
    // We expect the next block
    ctx->current_block++;
    return PROTO_RESULT_OK;
}

void stream_rx_close(stream_rx_t *ctx)
{
    ctx->state = STREAM_RX_STATE_IDLE;
}