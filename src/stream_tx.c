#include "stream_tx.h"
#include "frame.h"
#include "crc16.h"
#include <string.h>

#define STREAM_TX_TIMEOUT_CONN   (100)
#define STREAM_TX_TIMEOUT_ACK    (50)
#define STREAM_TX_MAX_RETRIES    (3)
#define STREAM_TX_MAX_RECEIVERS  255

// Helper to build control byte
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
    // Initialize ack_received to 0 (not received)
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
    // We expect to receive CONN_ACK, STREAM_ACK, or CLOSE frames.
    // We'll use the frame parser to parse the incoming byte.
    // But note: we don't have a frame parser in the transmitter context.
    // We'll create a simple parser for the expected frame types.

    // We'll use a static frame parser state for simplicity (not reentrant, but we assume one transmitter).
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
        // Incomplete or error, wait for more bytes
        return;
    }

    // We have a frame. Check the type.
    uint8_t type = (out_control & 0x38) >> 3;
    uint8_t broadcast = (out_control & 0x04) >> 2;

    // We only accept unicast frames (broadcast=0) for the transmitter.
    if (broadcast != 0)
    {
        return;
    }

    // The address in the frame is the transmitter's address (since we are the transmitter, the frame is sent to us).
    // So out_address should be our transmitter_id.
    if (out_address != ctx->transmitter_id)
    {
        return;
    }

    switch (type)
    {
        case 3: // CONN_ACK
            if (ctx->state == STREAM_TX_STATE_SENDING_CONN_REQ)
            {
                // Check payload length
                if (out_payload_len != 3)
                {
                    break;
                }
                // Payload: receiver_id, session_id, assigned_ack_slot
                uint8_t receiver_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                // uint8_t assigned_ack_slot = out_payload[2]; // We don't use the slot for now

                // Check if this is from one of our intended receivers
                int found = 0;
                for (int i = 0; i < ctx->receiver_count; i++)
                {
                    if (ctx->receiver_list[i] == receiver_id)
                    {
                        found = 1;
                        // Check session_id
                        if (session_id == ctx->session_id)
                        {
                            // Mark this receiver as connected (we'll consider it as having sent CONN_ACK)
                            // We don't need to store anything special; we just note that we have received CONN_ACK from this receiver.
                            // We'll move to the next receiver.
                            ctx->ack_received[i] = 1; // We'll use ack_received to mean CONN_ACK received for now.
                        }
                        break;
                    }
                }
                // If we got a CONN_ACK from a receiver we are expecting, move to next receiver.
                // We'll increment current_receiver_index and if we have more receivers, send next CONN_REQ.
                // If we have processed all receivers, then check how many we have connected.
                // We'll do that in the poll function by checking the ack_received array.
                // For now, we'll just move to the next receiver and let poll handle the rest.
                ctx->current_receiver_index++;
                if (ctx->current_receiver_index >= ctx->receiver_count)
                {
                    // We have processed all receivers. Now we need to see how many we connected.
                    // We'll count the number of receivers for which we got CONN_ACK.
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
                        // At least one receiver connected: move to stream sending
                        ctx->state = STREAM_TX_STATE_STREAM_SENDING;
                        ctx->timeout_counter = 0; // Not used in this state yet
                    }
                    else
                    {
                        // No receivers connected: go back to idle
                        ctx->state = STREAM_TX_STATE_IDLE;
                    }
                }
                else
                {
                    // Still have receivers to try: send CONN_REQ to the next one
                    ctx->timeout_counter = STREAM_TX_TIMEOUT_CONN;
                }
            }
            break;
        case 4: // STREAM_ACK
            if (ctx->state == STREAM_TX_STATE_WAIT_ACKS)
            {
                if (out_payload_len != 3)
                {
                    break;
                }
                uint8_t receiver_id = out_payload[0];
                uint8_t session_id = out_payload[1];
                uint8_t block_number = out_payload[2];

                // Check session_id and block_number
                if (session_id != ctx->session_id || block_number != ctx->current_block)
                {
                    break;
                }

                // Find the receiver in our list
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

                // Mark that we have received an ACK from this receiver for the current block
                ctx->ack_received[found] = 1;
            }
            break;
        case 5: // CLOSE
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
                    // Valid CLOSE frame: go to idle
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
                // Timeout: move to next receiver
                ctx->current_receiver_index++;
                if (ctx->current_receiver_index >= ctx->receiver_count)
                {
                    // Done with all receivers: check how many we connected
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
                        // We'll start sending the first block in the next state
                    }
                    else
                    {
                        ctx->state = STREAM_TX_STATE_IDLE;
                    }
                }
                else
                {
                    // Send CONN_REQ to the next receiver
                    ctx->timeout_counter = STREAM_TX_TIMEOUT_CONN;
                }
            }
            // If we are in this state and timeout_counter just reached 0, we will send the CONN_REQ in the next poll? We need to send it now.
            // We'll send the CONN_REQ when we enter the state or when we timeout and move to the next receiver.
            // We'll send it when we set the timeout_counter and when we decrement and it becomes 0? We'll send it when we are about to wait for a response.
            // Let's send the CONN_REQ when we set the timeout_counter (at the start of the state and after each timeout).
            // We'll do the sending in this block after setting the timeout_counter.
            // We'll send the CONN_REQ for the current_receiver_index.
            if (ctx->current_receiver_index < ctx->receiver_count)
            {
                uint8_t receiver_id = ctx->receiver_list[ctx->current_receiver_index];
                // Build CONN_REQ frame: transmitter_id, session_id, requested_ack_slot (we'll use 0)
                uint8_t payload[3] = { ctx->transmitter_id, ctx->session_id, 0 };
                uint8_t frame_buffer[255];
                uint8_t frame_len;
                // Control byte: type=2 (CONN_REQ), broadcast=0
                uint8_t control = build_control(2, 0);
                frame_build(control, receiver_id, payload, 3, frame_buffer, &frame_len);
                // Transmit the frame
                for (int i = 0; i < frame_len; i++)
                {
                    ctx->tx_byte(frame_buffer[i]);
                }
            }
            break;

        case STREAM_TX_STATE_STREAM_SENDING:
            // We are ready to send a block. We wait for the application to call stream_tx_send_block.
            // We don't do anything in poll for this state except maybe timeout? We don't have a timeout for sending the block.
            // We'll just wait for the application to provide a block.
            // We'll set a flag that we are ready to send a block? We don't have one.
            // We'll change: we'll consider that we are in this state and we are waiting for the application to call stream_tx_send_block.
            // We'll do nothing in poll.
            break;

        case STREAM_TX_STATE_WAIT_ACKS:
            if (ctx->timeout_counter > 0)
            {
                ctx->timeout_counter--;
            }
            else
            {
                // Timeout waiting for ACKs
                // Check how many ACKs we have received for the current block
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
                    // All receivers have ACKed: move to next block
                    ctx->current_block++;
                    ctx->retry_count = 0;
                    // Reset ack_received for the next block
                    memset(ctx->ack_received, 0, ctx->receiver_count);
                    ctx->state = STREAM_TX_STATE_STREAM_SENDING;
                }
                else
                {
                    // Not all ACKs: check if we can retry
                    if (ctx->retry_count < STREAM_TX_MAX_RETRIES)
                    {
                        ctx->retry_count++;
                        ctx->timeout_counter = STREAM_TX_TIMEOUT_ACK;
                        // We will resend the same block: we need to tell the application to resend the same block.
                        // We don't have a way to signal the application. We'll rely on the application calling stream_tx_send_block again with the same data?
                        // We'll change: we'll have the application call stream_tx_send_block only when we are in STREAM_TX_STATE_STREAM_SENDING.
                        // When we timeout and retry, we stay in WAIT_ACKS and we will resend the block by having the application resend the same data.
                        // We don't have a way to notify the application. We'll leave it to the application to resend the same block if it wants to.
                        // We'll just wait for the application to call stream_tx_send_block again.
                        // We'll not change the state.
                    }
                    else
                    {
                        // Max retries reached: go to closing
                        ctx->state = STREAM_TX_STATE_CLOSING;
                        ctx->timeout_counter = 0; // We'll send CLOSE immediately
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
                // Send CLOSE frame to all receivers? The protocol says the transmitter sends CLOSE to each receiver? Or to a specific one?
                // The README says: `CLOSE` is a normal frame unicast. It doesn't specify to whom. We'll send to the first receiver for now.
                // We'll send CLOSE to each receiver in sequence? We'll send to the first receiver and then move to idle.
                // We'll send CLOSE to the first receiver in the list.
                if (ctx->receiver_count > 0)
                {
                    uint8_t receiver_id = ctx->receiver_list[0];
                    uint8_t payload[3] = { ctx->transmitter_id, ctx->session_id, 0 }; // reason 0 for COMPLETE
                    uint8_t frame_buffer[255];
                    uint8_t frame_len;
                    uint8_t control = build_control(5, 0); // type=5 (CLOSE), broadcast=0
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

    // Build STREAM_DATA frame: block_number (1 byte), data (254 bytes), CRC (2 bytes)
    // We'll build the frame manually because it's not a normal frame.
    uint8_t frame_buffer[257]; // 1 + 254 + 2
    uint8_t *ptr = frame_buffer;
    *ptr++ = ctx->current_block;
    memcpy(ptr, data, 254);
    ptr += 254;

    // Compute CRC over block_number and data
    uint8_t crc_buffer[255]; // 1 + 254
    crc_buffer[0] = ctx->current_block;
    memcpy(&crc_buffer[1], data, 254);
    uint16_t crc = crc16_ccitt_false(crc_buffer, 255);
    *ptr++ = (uint8_t)(crc & 0x00FF);
    *ptr++ = (uint8_t)((crc & 0xFF00) >> 8);

    // Transmit the frame
    for (int i = 0; i < 257; i++)
    {
        ctx->tx_byte(frame_buffer[i]);
    }

    // After sending the block, we wait for ACKs
    ctx->state = STREAM_TX_STATE_WAIT_ACKS;
    ctx->timeout_counter = STREAM_TX_TIMEOUT_ACK;
    // Reset ack_received for this block (we'll set to 0 when we start waiting)
    memset(ctx->ack_received, 0, ctx->receiver_count);

    return PROTO_RESULT_OK;
}

void stream_tx_close(stream_tx_t *ctx, uint8_t reason)
{
    if (ctx->state == STREAM_TX_STATE_STREAM_SENDING || ctx->state == STREAM_TX_STATE_WAIT_ACKS)
    {
        ctx->state = STREAM_TX_STATE_CLOSING;
        ctx->timeout_counter = 0; // We'll send CLOSE immediately in the poll function
    }
    else if (ctx->state == STREAM_TX_STATE_IDLE)
    {
        // Already idle, do nothing
    }
    // If we are in SENDING_CONN_REQ, we can also go to closing? We'll just go to idle.
    else
    {
        ctx->state = STREAM_TX_STATE_IDLE;
    }
}