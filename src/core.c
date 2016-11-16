#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <glib.h>
#include <poll.h>

#include "utils.h"
#include "core.h"
#include "_cgo_export.h"

struct raw_touchpad_event {
    double dx_unaccel, dy_unaccel;
    double scale;
    int fingers;
};

static int is_touchpad(struct libinput_device *dev);
static const char* get_multitouch_device_node(struct libinput_event *ev, int *cap_type);

static void handle_events(struct libinput *li);
static void handle_device_added(struct libinput_event *ev);
static void handle_device_removed(struct libinput_event *ev);
static void handle_gesture_events(struct libinput_event *ev, int type);

static GHashTable *tp_table = NULL;
static GHashTable *ts_table = NULL;

int
start_loop()
{
    struct libinput *li = open_from_udev("seat0", NULL, 0);
    if (!li) {
        return -1;
    }

    tp_table = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     (GDestroyNotify)g_free,
                                     (GDestroyNotify)g_free);
    if (!tp_table) {
        fprintf(stderr, "Failed to initialize touchpad table\n");
        libinput_unref(li);
        return -1;
    }

    ts_table = g_hash_table_new_full(g_str_hash,
                                     g_str_equal,
                                     (GDestroyNotify)g_free,
                                     (GDestroyNotify)g_list_free);
    if (!ts_table) {
        fprintf(stderr, "Failed to initialize touchscreen table\n");
        g_hash_table_destroy(tp_table);
        libinput_unref(li);
        return -1;
    }

    // firstly handle all devices
    handle_events(li);

    struct pollfd fds;
    fds.fd = libinput_get_fd(li);
    fds.events = POLLIN;
    fds.revents = 0;

    while(poll(&fds, 1, -1) > -1) {
        handle_events(li);
    }

    return 0;
}

static const char*
get_multitouch_device_node(struct libinput_event *ev, int *cap_type)
{
    struct libinput_device *dev = libinput_event_get_device(ev);
    if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TOUCH)) {
        *cap_type = LIBINPUT_DEVICE_CAP_TOUCH;
        goto out;
    } else if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER) &&
               is_touchpad(dev)) {
        *cap_type = LIBINPUT_DEVICE_CAP_POINTER;
        goto out;
    } else {
        return NULL;
    }

out:
    return udev_device_get_devnode(libinput_device_get_udev_device(dev));
}

static int
is_touchpad(struct libinput_device *dev)
{
    // TODO: check touchpad whether support multitouch. fingers > 3?
    int cnt = libinput_device_config_tap_get_finger_count(dev);
    return (cnt > 0);
}

static void
handle_device_added(struct libinput_event *ev)
{
    int cap = 0;
    const char *path = get_multitouch_device_node(ev, &cap);
    if (!path) {
        return;
    }

    printf("Device added: %s\n", path);
    if (cap == LIBINPUT_DEVICE_CAP_POINTER) {
        g_hash_table_insert(tp_table, g_strdup(path),
                            g_new0(struct raw_touchpad_event, 1));
    } else {
        g_hash_table_insert(ts_table, g_strdup(path),
                            g_new0(GList, 1));
    }
}

static void
handle_device_removed(struct libinput_event *ev)
{
    int cap = 0;
    const char *path = get_multitouch_device_node(ev, &cap);
    if (!path) {
        return;
    }

    printf("Will remove '%s' to table\n", path);
    if (cap == LIBINPUT_DEVICE_CAP_POINTER) {
        g_hash_table_remove(tp_table, path);
    } else {
        g_hash_table_remove(ts_table, path);
    }
}

/**
 * calculation direction
 * Swipe: (begin -> end)
 *     _dx_unaccel += dx_unaccel, _dy_unaccel += dy_unaccel;
 *     filter small movement threshold abs(_dx_unaccel - _dy_unaccel) < 70
 *     if abs(_dx_unaccel) > abs(_dy_unaccel): _dx_unaccel < 0 ? 'left':'right'
 *     else: _dy_unaccel < 0 ? 'up':'down'
 *
 * Pinch: (begin -> end)
 *     _scale += 1.0 - scale;
 *     if _scale != 0: _scale >= 0 ? 'in':'out'
 **/
static void
handle_gesture_events(struct libinput_event *ev, int type)
{
    struct libinput_device *dev = libinput_event_get_device(ev);
    if (!dev) {
        fprintf(stderr, "Get device from event failure\n");
        return ;
    }

    const char *node = udev_device_get_devnode(libinput_device_get_udev_device(dev));
    struct raw_touchpad_event *raw = g_hash_table_lookup(tp_table, node);
    if (!raw) {
        fprintf(stderr, "Not found '%s' in table\n", node);
        return ;
    }
    struct libinput_event_gesture *gesture = libinput_event_get_gesture_event(ev);
    switch (type) {
    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
        // reset
        raw->dx_unaccel = 0.0;
        raw->dy_unaccel = 0.0;
        raw->scale = 0.0;
        raw->fingers = 0;
        break;
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:{
        double scale = libinput_event_gesture_get_scale(gesture);
        raw->scale += 1.0-scale;
        break;
    }
    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:{
        // update
        double dx_unaccel = libinput_event_gesture_get_dx_unaccelerated(gesture);
        double dy_unaccel = libinput_event_gesture_get_dy_unaccelerated(gesture);
        raw->dx_unaccel += dx_unaccel;
        raw->dy_unaccel += dy_unaccel;
        break;
    }
    case LIBINPUT_EVENT_GESTURE_PINCH_END:{
        // do action
        if (raw->scale == 0) {
            break;
        }

        raw->fingers = libinput_event_gesture_get_finger_count(gesture);
        printf("[Pinch] direction: %s, fingers: %d\n",
               raw->scale>= 0?"in":"out", raw->fingers);
        handleGestureEvent(GESTURE_TYPE_PINCH,
                           (raw->scale >= 0?GESTURE_DIRECTION_IN:GESTURE_DIRECTION_OUT),
                           raw->fingers);
        break;
    }
    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
        // filter small movement threshold
        if (fabs(raw->dx_unaccel - raw->dy_unaccel) < 70) {
            break;
        }

        raw->fingers = libinput_event_gesture_get_finger_count(gesture);
        if (fabs(raw->dx_unaccel) > fabs(raw->dy_unaccel)) {
            // right/left movement
            printf("[Swipe] direction: %s, fingers: %d\n",
                   raw->dx_unaccel < 0?"left":"right", raw->fingers);
            handleGestureEvent(GESTURE_TYPE_SWIPE,
                               (raw->dx_unaccel < 0?GESTURE_DIRECTION_LEFT:GESTURE_DIRECTION_RIGHT),
                               raw->fingers);
        } else {
            // up/down movement
            printf("[Swipe] direction: %s, fingers: %d\n",
                   raw->dy_unaccel < 0?"up":"down", raw->fingers);
            handleGestureEvent(GESTURE_TYPE_SWIPE,
                               (raw->dy_unaccel < 0?GESTURE_DIRECTION_UP:GESTURE_DIRECTION_DOWN),
                               raw->fingers);
        }
        break;
    }
}

static void
handle_touch_events(struct libinput_event *ev, int ty)
{
    if (ty == LIBINPUT_EVENT_TOUCH_FRAME || ty == LIBINPUT_EVENT_TOUCH_CANCEL) {
        return;
    }

    const char *node = udev_device_get_devnode(libinput_device_get_udev_device(libinput_event_get_device(ev)));
    struct libinput_event_touch *touch = libinput_event_get_touch_event(ev);
    uint64_t sec = libinput_event_touch_get_time(touch);
    uint64_t usec = libinput_event_touch_get_time_usec(touch);
    if (ty == LIBINPUT_EVENT_TOUCH_UP) {
        // sum the all events, send gesture event
        printf("Touch up timestamp: %ld, usec: %ld\n", sec, usec);
        return;
    }

    double x = libinput_event_touch_get_x(touch);
    double y = libinput_event_touch_get_y(touch);
    printf("\tX: %lf, Y: %lf, Timestamp: %ld, usec: %ld\n", x, y, sec, usec);
}

static void
handle_events(struct libinput *li)
{
    struct libinput_event *ev;
    libinput_dispatch(li);
    while ((ev = libinput_get_event(li))) {
        int type =libinput_event_get_type(ev);
        switch (type) {
        case LIBINPUT_EVENT_DEVICE_ADDED:{
            handle_device_added(ev);
            break;
        }
        case LIBINPUT_EVENT_DEVICE_REMOVED: {
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
        case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
        case LIBINPUT_EVENT_GESTURE_PINCH_END:
        case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
        case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
        case LIBINPUT_EVENT_GESTURE_SWIPE_END:{
            handle_gesture_events(ev, type);
            break;
        }
        case LIBINPUT_EVENT_POINTER_AXIS:{
            printf("[Event pointer axis]\n");
            break;
        }
        case LIBINPUT_EVENT_TOUCH_MOTION:
        case LIBINPUT_EVENT_TOUCH_UP:
        case LIBINPUT_EVENT_TOUCH_DOWN:
        case LIBINPUT_EVENT_TOUCH_FRAME:
        case LIBINPUT_EVENT_TOUCH_CANCEL:{
            handle_touch_events(ev, type);
            break;
        }
        default:
            break;
        }
        libinput_event_destroy(ev);
        libinput_dispatch(li);
    }
}
