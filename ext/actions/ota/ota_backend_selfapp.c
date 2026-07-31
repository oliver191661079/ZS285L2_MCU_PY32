#include <msg_manager.h>
#include <sys_manager.h>

#include <ota_backend_selfapp.h>

typedef struct {
    struct ota_backend  backend;
    ota_event_cb_t  dfucb;
} backend_selfapp_t;

static backend_selfapp_t   selfapp_context;
static backend_selfapp_t  *self = NULL;

static inline u8_t backendself_inited(void)
{
    if (self) {
        return (self->backend.type == OTA_BACKEND_TYPE_SELFAPP);
    }
    return 0;
}

static void backendself_api_init(struct ota_backend_api *api)
{
    if (backendself_inited()) {
        self->backend.api = api;
    }
}

static void backendself_api_deinit(void)
{
    if (backendself_inited()) {
        self->backend.api = NULL;
    }
}

// - - - Called by ota_app - - - //

struct ota_backend *selfapp_backend_init(ota_backend_notify_cb_t cb)
{
    if (backendself_inited()) {
        printk("selfbackend inited\n");
        return &(self->backend);
    }

    self = &selfapp_context;
    ota_backend_init(&(self->backend), OTA_BACKEND_TYPE_SELFAPP, NULL, cb);
    self->dfucb = NULL;

    return &(self->backend);
}

void selfapp_backend_callback(int event)
{
    if (backendself_inited() && self->dfucb) {
        self->dfucb(event);
    }
}

// - - - Called by btself_app - - - //

int selfapp_backend_otadfu_start(struct ota_backend_api *api, ota_event_cb_t dfucb)
{
    if (backendself_inited()) {
        backendself_api_init(api);
        self->dfucb = dfucb;
        self->backend.cb(&(self->backend), OTA_BACKEND_UPGRADE_STATE, 1);
        printk("selfbackend start\n");
        return 0;
    }

    return -1;
}

int selfapp_backend_otadfu_stop(void)
{
    if (backendself_inited()) {
        self->backend.cb(&(self->backend), OTA_BACKEND_UPGRADE_STATE, 0);
        self->dfucb = NULL;
        backendself_api_deinit();
        printk("selfbackend stop\n");
        return 0;
    }

    return -1;
}


