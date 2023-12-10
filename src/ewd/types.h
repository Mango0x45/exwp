#ifndef WLBG_TYPES_H
#define WLBG_TYPES_H

typedef int32_t i32;
typedef uint32_t u32;

typedef struct wl_buffer wl_buffer_t;
typedef struct wl_output wl_output_t;
typedef struct wl_region wl_region_t;
typedef struct wl_registry wl_registry_t;
typedef struct wl_shm_pool wl_shm_pool_t;
typedef struct wl_shm wl_shm_t;
typedef struct wl_surface wl_surface_t;
typedef struct zwlr_layer_surface_v1 zwlr_layer_surface_v1_t;

typedef struct wl_buffer_listener wl_buffer_listener_t;
typedef struct wl_output_listener wl_output_listener_t;
typedef struct wl_registry_listener wl_registry_listener_t;
typedef struct wl_shm_listener wl_shm_listener_t;
typedef struct zwlr_layer_surface_v1_listener zwlr_layer_surface_v1_listener_t;

#endif /* !WLBG_TYPES_H */
