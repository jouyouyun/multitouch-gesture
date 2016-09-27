#include <libinput.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stropts.h>
#include <signal.h>
#include <poll.h>

#include "gesture.h"


static int open_device(const struct libinput_interface *interface, void *user_data,
                       const char *path, int verbose);
static void log_handler(struct libinput *li, enum libinput_log_priority priority,
                        const char* format, va_list args);
static int open_restricted(const char *path, int flags, void *user_data);
static void close_restricted(int fd, void *user_data);

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};
static struct libinput *input_hander = NULL;

int
register_device(char *device, GESTURE_CALLBACK handler, int verbose)
{
    if (!device || !handler) {
        fprintf(stderr, "Invalid path '%s' or handler '%p'\n", device, handler);
        return -1;
    }

    int ret = open_device(&interface, NULL, device, verbose);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

static void
print_pointer_axis_event(struct libinput_event *ev)
{
    struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
    double v = 0, h = 0;
    const char *vert = "", *horiz = "";
    if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        v = libinput_event_pointer_get_axis_value(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        vert = "*";
    }
    if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        h = libinput_event_pointer_get_axis_value(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
    }
    printf("vert: %.2f%s, horiz: %0.2f%s\n", v, vert, h, horiz);
}

static void
print_gesture_event(struct libinput_event *ev)
{
    struct libinput_event_gesture *g = libinput_event_get_gesture_event(ev);
    double dx = libinput_event_gesture_get_dx(g);
    double dy = libinput_event_gesture_get_dy(g);
    double dx_unaccel = libinput_event_gesture_get_dx_unaccelerated(g);
    double dy_unaccel = libinput_event_gesture_get_dy_unaccelerated(g);

    printf("fingers (%d) %5.2f/%5.2f (%5.2f/%5.2f unaccelerated)\n",
           libinput_event_gesture_get_finger_count(g),
           dx, dy, dx_unaccel, dy_unaccel);
}

static int
handle_events()
{
    int ret = -1;
    struct libinput_event *ev;

    libinput_dispatch(input_hander);
    while ((ev = libinput_get_event(input_hander))) {
        switch (libinput_event_get_type(ev)) {
        case LIBINPUT_EVENT_POINTER_AXIS:{
            printf("[Pointer Axis Event]");
            print_pointer_axis_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:{
            printf("[Gesture Swipe Begin]");
            print_gesture_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:{
            printf("[Gesture Swipe Update]");
            print_gesture_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_END:{
            printf("[Gesture Swipe End]");
            print_gesture_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:{
            printf("[Gesture Pinch Begin]");
            print_gesture_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:{
            printf("[Gesture Pinch Update]");
            print_gesture_event(ev);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_PINCH_END:{
            printf("[Gesture Pinch End]");
            print_gesture_event(ev);
            break;
        }
        default:
            break;
        }

        libinput_event_destroy(ev);
        libinput_dispatch(input_hander);
        ret = 0;
    }
    return ret;
}

static int stop = 0;

static void
sighandler(int signal, siginfo_t *siginfo, void *user_data)
{
    quit_loop();
}

int
start_loop()
{
    if (!input_hander) {
        fprintf(stderr, "No device register\n");
        return -1;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = sighandler;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        fprintf(stderr, "Failed to set signal handler (%s)\n", strerror(errno));
        return -1;
    }

    if (handle_events() != 0) {
        fprintf(stderr, "Expected device added events on startup but got none. "
                "Maybe you don't have the right permissions?\n");
    }

    struct  pollfd fds;

    fds.fd = libinput_get_fd(input_hander);
    fds.events = POLLIN;
    fds.revents = 0;

    printf("------------Loop\n");
    while (!stop && (poll(&fds, 1, -1) > -1)) {
        handle_events();
    }

    close(fds.fd);
    libinput_unref(input_hander);
    input_hander = NULL;
    return 0;
}

void
quit_loop()
{
    stop = 1;
}

static int
open_device(const struct libinput_interface *interface, void *user_data,
            const char *path, int verbose)
{
    if (!input_hander) {
        input_hander = libinput_path_create_context(interface, user_data);
        if (!input_hander) {
            input_hander = NULL;
            fprintf(stderr, "Create context failed from: %s\n", path);
            return -1;
        }
    }

    // TODO: as a function
    if (verbose) {
        libinput_log_set_priority(input_hander, LIBINPUT_LOG_PRIORITY_DEBUG);
        libinput_log_set_handler(input_hander, log_handler);
    }

    struct libinput_device *device = libinput_path_add_device(input_hander, path);
    if (!device) {
        fprintf(stderr, "Initialized device '%s' failed\n", path);
        return -1;
    }

    return 0;
}

static void
log_handler(struct libinput *li, enum libinput_log_priority priority,
            const char* format, va_list args)
{
    vprintf(format, args);
}

static int
open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    if (fd < 0) {
        fprintf(stderr, "Failed to open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return fd;
}

static void
close_restricted(int fd, void *user_data)
{
    close(fd);
}
