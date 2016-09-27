#include <stdio.h>

#include "gesture.h"

static void
touch_gesture_handler(TouchGesture detail)
{
    printf("TODO: handle gesture: %d\n", detail.gesture);
}

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <device path>\n", argv[0]);
        return -1;
    }

    int ret = register_device(argv[1], touch_gesture_handler, 0);
    if (ret != 0) {
        return ret;
    }

    ret = start_loop();
    return ret;
}
