#include "modbus_lora.h"
#include "modbus-private.h"
#include <mutex>
#include <queue>
#include <vector>
#include <unistd.h>
#include <cstring>
#include <iostream>

// External declarations to access lora_app's queue and transmit function
struct RxMessage {
  std::vector<uint8_t> data;
};
extern std::queue<RxMessage> message_queue;
extern std::mutex queue_mutex;
extern bool transmit(const uint8_t *payload, uint8_t size, sx126x_pkt_params_lora_t *pkt_params);

struct modbus_lora_data_t {
    sx126x_pkt_params_lora_t *pkt_params;
    std::vector<uint8_t> rx_buffer;
    size_t rx_offset;
};

static int _modbus_lora_set_slave(modbus_t *ctx, int slave) {
    ctx->slave = slave;
    return 0;
}

static int _modbus_lora_build_request_basis(modbus_t *ctx, int function, int addr, int nb, uint8_t *req) {
    req[0] = ctx->slave;
    req[1] = function;
    req[2] = addr >> 8;
    req[3] = addr & 0x00ff;
    req[4] = nb >> 8;
    req[5] = nb & 0x00ff;
    return 6;
}

static int _modbus_lora_build_response_basis(sft_t *sft, uint8_t *rsp) {
    rsp[0] = sft->slave;
    rsp[1] = sft->function;
    return 2;
}

static int _modbus_lora_get_response_tid(const uint8_t *req) {
    return 0;
}

static int _modbus_lora_send_msg_pre(uint8_t *req, int req_length) {
    return req_length;
}

static ssize_t _modbus_lora_send(modbus_t *ctx, const uint8_t *req, int req_length) {
    modbus_lora_data_t *data = (modbus_lora_data_t *)ctx->backend_data;
    if (transmit(req, req_length, data->pkt_params)) {
        return req_length;
    }
    return -1;
}

static int _modbus_lora_receive(modbus_t *ctx, uint8_t *req) {
    return _modbus_receive_msg(ctx, req, MSG_INDICATION);
}

static ssize_t _modbus_lora_recv(modbus_t *ctx, uint8_t *rsp, int rsp_length) {
    modbus_lora_data_t *data = (modbus_lora_data_t *)ctx->backend_data;
    size_t available = data->rx_buffer.size() - data->rx_offset;
    size_t to_copy = (available < (size_t)rsp_length) ? available : (size_t)rsp_length;
    if (to_copy > 0) {
        memcpy(rsp, data->rx_buffer.data() + data->rx_offset, to_copy);
        data->rx_offset += to_copy;
    }
    return to_copy;
}

static int _modbus_lora_check_integrity(modbus_t *ctx, uint8_t *msg, const int msg_length) {
    return msg_length; // Assuming LoRa handles CRC or integrity check
}

static int _modbus_lora_pre_check_confirmation(modbus_t *ctx, const uint8_t *req, const uint8_t *rsp, int rsp_length) {
    if (req[0] != rsp[0] && req[0] != MODBUS_BROADCAST_ADDRESS) {
        errno = EMBBADSLAVE;
        return -1;
    }
    return 0;
}

static int _modbus_lora_connect(modbus_t *ctx) {
    return 0; 
}

static unsigned int _modbus_lora_is_connected(modbus_t *ctx) {
    return 1;
}

static void _modbus_lora_close(modbus_t *ctx) {
}

static int _modbus_lora_flush(modbus_t *ctx) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while(!message_queue.empty()) message_queue.pop();
    modbus_lora_data_t *data = (modbus_lora_data_t *)ctx->backend_data;
    data->rx_buffer.clear();
    data->rx_offset = 0;
    return 0;
}

static int _modbus_lora_select(modbus_t *ctx, fd_set *rset, struct timeval *tv, int msg_length) {
    modbus_lora_data_t *data = (modbus_lora_data_t *)ctx->backend_data;
    
    if (data->rx_buffer.size() - data->rx_offset > 0) {
        return 1;
    }

    long timeout_ms = tv ? (tv->tv_sec * 1000 + tv->tv_usec / 1000) : -1;
    long elapsed_ms = 0;
    while (timeout_ms == -1 || elapsed_ms < timeout_ms) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!message_queue.empty()) {
                RxMessage msg = message_queue.front();
                message_queue.pop();
                data->rx_buffer = msg.data;
                data->rx_offset = 0;
                return 1;
            }
        }
        usleep(10000); // 10ms
        elapsed_ms += 10;
    }
    return 0; 
}

static void _modbus_lora_free(modbus_t *ctx) {
    delete (modbus_lora_data_t *)ctx->backend_data;
    free(ctx);
}

const modbus_backend_t _modbus_lora_backend = {
    _MODBUS_BACKEND_TYPE_RTU, 
    1,                        
    0,                        
    256,                      
    _modbus_lora_set_slave,
    _modbus_lora_build_request_basis,
    _modbus_lora_build_response_basis,
    _modbus_lora_get_response_tid,
    _modbus_lora_send_msg_pre,
    _modbus_lora_send,
    _modbus_lora_receive,
    _modbus_lora_recv,
    _modbus_lora_check_integrity,
    _modbus_lora_pre_check_confirmation,
    _modbus_lora_connect,
    _modbus_lora_is_connected,
    _modbus_lora_close,
    _modbus_lora_flush,
    _modbus_lora_select,
    _modbus_lora_free
};

extern "C" modbus_t* modbus_new_lora(sx126x_pkt_params_lora_t *pkt_params) {
    modbus_t *ctx = (modbus_t *)malloc(sizeof(modbus_t));
    if (ctx == nullptr) return nullptr;

    _modbus_init_common(ctx);
    ctx->backend = &_modbus_lora_backend;
    ctx->backend_data = new modbus_lora_data_t();
    ((modbus_lora_data_t *)ctx->backend_data)->pkt_params = pkt_params;
    ((modbus_lora_data_t *)ctx->backend_data)->rx_offset = 0;

    return ctx;
}
