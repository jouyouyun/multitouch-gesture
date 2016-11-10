#ifndef __GESTURE_UTILS_H__
#define __GESTURE_UTILS_H__

#include <libinput.h>

struct libinput* open_from_udev(char *seat, void *user_data, int verbose);
struct libinput* open_from_path(char **path, void *user_data, int verbose);

void free_strv(char **list);

#endif
