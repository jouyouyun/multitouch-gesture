#ifndef __GESTURE_H__
#define __GESTURE_H__

#define GestureTypeSwipe 10
#define GestureTypePinch 11

#define GestureDirectionUp 100
#define GestureDirectionDown 101
#define GestureDirectionLeft 102
#define GestureDirectionRight 103
#define GestureDirectionIn 104
#define GestureDirectionOut 105

typedef struct _TouchGesture {
    int fingers; // The number of fingers
    int gesture; // The type of gesture
    int direction; // The direction of gesture
} TouchGesture;

typedef void (*GESTURE_CALLBACK) (TouchGesture info);

// Register handler for the special event device
int register_device(char *device, GESTURE_CALLBACK handler, int verbose);

// Loop
int start_loop();
void quit_loop();

#endif
