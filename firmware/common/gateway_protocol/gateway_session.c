/* Pairing and liveness rules, independent of physical SPI and the scheduler. */
#include "gateway_protocol.h"

void gw_session_init(gw_session_t *session, uint64_t local_session)
{
    *session = (gw_session_t){.local = local_session};
}
gw_accept_t gw_session_accept(gw_session_t *session, const gw_frame_t *frame, uint64_t now_ms)
{
    if (frame->type == GW_HELLO && frame->sender != session->peer)
    {
        if (frame->peer && frame->peer != session->local)
        {
            return GW_REJECT;
        }
        /* Do not silently replace a live peer: the owner must close the gate first. */
        if (session->ready)
        {
            return GW_PEER_CHANGED;
        }
        session->peer = frame->sender;
        session->received_sequence = 0;
    }
    if (!session->peer || frame->sender != session->peer ||
        frame->sequence <= session->received_sequence ||
        (frame->peer != session->local && !(frame->type == GW_HELLO && !frame->peer)) ||
        (frame->type == GW_DATA && !session->ready))
    {
        return GW_REJECT;
    }
    /* Only accepted traffic can refresh the timeout. */
    session->received_sequence = frame->sequence;
    session->last_rx_ms = now_ms;
    if (frame->peer == session->local)
    {
        session->ready = true;
    }
    return GW_ACCEPT;
}
bool gw_session_expired(const gw_session_t *session, uint64_t now_ms)
{
    return session->peer && now_ms - session->last_rx_ms >= GW_TIMEOUT_MS;
}
