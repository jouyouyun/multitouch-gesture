#include <stdio.h>
#include <string.h>

#include <poll.h>

#include "utils.h"

static int is_touchpad(struct libinput_device *dev);
static const char* get_device_node(struct libinput_event *ev);

char **
list_multitouch_devices()
{
    struct libinput *li = open_from_udev(NULL, NULL, 0);
    if (!li) {
        return NULL;
    }

    char **list = NULL;
    int cnt = 0;
    struct libinput_event *ev = NULL;
    libinput_dispatch(li);
    while ((ev = libinput_get_event(li))) {
        if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED) {
            const char *path = get_device_node(ev);
            if (path) {
                list = realloc(list, cnt+1 * sizeof(char*));
                list[cnt] = strdup(path);
                cnt++;
            }
        }
        libinput_event_destroy(ev);
        libinput_dispatch(li);
    }
    libinput_unref(li);
    list = realloc(list, cnt+1 * sizeof(char*));
    list[cnt] = NULL;
    return list;
}

static const char*
get_device_node(struct libinput_event *ev)
{
    struct libinput_device *dev = libinput_event_get_device(ev);
    if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TOUCH)) {
        goto out;
    } else if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER) &&
               is_touchpad(dev)) {
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

int
main()
{
    char **list = list_multitouch_devices();
    if (!list) {
        fprintf(stderr, "No multitouch device found\n");
        return -1;
    }

    struct libinput *li = open_from_path(list, NULL, 0);
    free_strv(list);
    if (!li) {
        return -1;
    }

    struct pollfd fds;
    fds.fd = libinput_get_fd(li);
    fds.events = POLLIN;
    fds.revents = 0;

    while(poll(&fds, 1, -1) > -1) {
        struct libinput_event *ev = NULL;
        libinput_dispatch(li);
        while ((ev = libinput_get_event(li))) {
            struct libinput_device *dev = libinput_event_get_device(ev);
            printf("Name: \t%s\n"
                   "Type: %d\n"
                   "Kernel: \t%s\n", libinput_device_get_name(dev),
                   libinput_event_get_type(ev),
                   udev_device_get_devnode(libinput_device_get_udev_device(dev)));
            libinput_event_destroy(ev);
            libinput_dispatch(li);
        }
    }
}
