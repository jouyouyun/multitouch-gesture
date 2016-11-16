package main

// #cgo pkg-config: libinput glib-2.0
// #cgo LDFLAGS: -ludev -lm
// #include <stdlib.h>
// #include <libinput.h>
// #include "core.h"
import "C"

import (
	"fmt"
	"math"
	"os/exec"
	"pkg.deepin.io/lib/dbus"
)

const (
	dbusDest = "com.deepin.daemon.Gesture"
	dbusPath = "/com/deepin/daemon/Gesture"
	dbusIFC  = "com.deepin.daemon.Gesture"
)

type Manager struct {
	// name, direction, fingers
	Event func(int32, int32, int32)
}

type touchEventInfo struct {
	Type      int32 // down, motion or up
	Timestamp uint64
	X         float64
	Y         float64
}
type touchEventInfos []*touchEventInfo

type touchEventManager struct {
	Fingers int32
	Infos   touchEventInfos
}

var (
	tsTypeUp     = int32(C.LIBINPUT_EVENT_TOUCH_UP)
	tsTypeDown   = int32(C.LIBINPUT_EVENT_TOUCH_DOWN)
	tsTypeMotion = int32(C.LIBINPUT_EVENT_TOUCH_MOTION)
)

var (
	_m          = &Manager{}
	tsEvHandler = make(map[string]touchEventManager)
)

func (*Manager) GetDBusInfo() dbus.DBusInfo {
	return dbus.DBusInfo{
		Dest:       dbusDest,
		ObjectPath: dbusPath,
		Interface:  dbusIFC,
	}
}

//export handleGestureEvent
func handleGestureEvent(name, direction, fingers C.int) {
	dbus.Emit(_m, "Event", int32(name), int32(direction), int32(fingers))
}

//export handleTouchEvents
func handleTouchEvents(node *C.char, x, y C.double, evType C.int, timestamp C.uint) {
	var info = touchEventInfo{
		Type:      int32(evType),
		Timestamp: uint64(timestamp),
		X:         float64(x),
		Y:         float64(y),
	}

	name := C.GoString(node)
	manager, ok := tsEvHandler[name]
	fmt.Println("++++++++++++++++Event type:", evType, ok)
	if !ok {
		if info.Type == tsTypeDown {
			tsEvHandler[name] = touchEventManager{
				Fingers: 1,
				Infos:   touchEventInfos{&info},
			}
		}
		return
	}

	len := len(manager.Infos)
	if info.Type == tsTypeDown {
		fmt.Println("--------------Touch down")
		// 不是连续的 touch down 事件, 丢弃这些事件
		if manager.Infos[len-1].Type != tsTypeDown {
			delete(tsEvHandler, name)
			return
		}

		if manager.Infos[len-1].Type == tsTypeDown {
			manager.Fingers += 1
		}
	} else if info.Type == tsTypeMotion {
		if manager.Infos[len-1].Type != tsTypeDown {
			// 判断手势是长按还是拖拽, 根据 x, y 偏移判断, 10px 以内
			if manager.Fingers == 1 {
				dx := info.X - manager.Infos[1].X
				dy := info.Y - manager.Infos[1].Y
				if math.Abs(dx) > 10 || math.Abs(dy) > 10 {
					// Drag
				} else {
					duration := info.Timestamp - manager.Infos[0].Timestamp
					fmt.Println("Motion duration:", duration)
					// 500ms
					if duration > 300 {
						// 丢弃事件
						delete(tsEvHandler, name)
						fmt.Println("Long pressed, right button")
						out, err := exec.Command("/bin/sh", "-c",
							"xdotool click 3").CombinedOutput()
						if err != nil {
							fmt.Println("Right click failed:", string(out), err)
						}
						return
					}
				}
			}
		}
	} else if info.Type == tsTypeUp {
		// 主要处理多个手指按下的事件, 单指点击忽略
		if manager.Fingers > 1 {
		}
		delete(tsEvHandler, name)
		return
	}

	manager.Infos = append(manager.Infos, &info)
	tsEvHandler[name] = manager
}

func main() {
	err := dbus.InstallOnSystem(_m)
	if err != nil {
		fmt.Println("Install session bus failed:", err)
		return
	}
	dbus.DealWithUnhandledMessage()

	C.start_loop()
}
