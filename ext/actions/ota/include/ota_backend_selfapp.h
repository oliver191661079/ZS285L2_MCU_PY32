
#include <ota_backend.h>

typedef void (*ota_event_cb_t)(int event);

extern struct ota_backend *selfapp_backend_init(ota_backend_notify_cb_t cb);
extern void selfapp_backend_callback(int event);

extern int selfapp_backend_otadfu_start(struct ota_backend_api *api, ota_event_cb_t dfucb);
extern int selfapp_backend_otadfu_stop(void);
