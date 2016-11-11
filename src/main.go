package main

// #cgo pkg-config: libinput glib-2.0
// #cgo LDFLAGS: -ludev -lm
// #include <stdlib.h>
// #include "core.h"
import "C"

import (
	"fmt"
	"pkg.deepin.io/lib/dbus"
)

const (
	dbusDest = "com.deepin.daemon.Gesture"
	dbusPath = "/com/deepin/daemon/Gesture"
	dbusIFC  = "com.deepin.daemon.Gesture"
)

type Manager struct {
	// gesture name, gesture action or direction, fingers
	Event func(int32, int32, int32)
}

var _m = &Manager{}

func (*Manager) GetDBusInfo() dbus.DBusInfo {
	return dbus.DBusInfo{
		Dest:       dbusDest,
		ObjectPath: dbusPath,
		Interface:  dbusIFC,
	}
}

//export handleGestureEvent
func handleGestureEvent(name, action, fingers C.int) {
	dbus.Emit(_m, "Event", int32(name), int32(action), int32(fingers))
}

func main() {
	err := dbus.InstallOnSession(_m)
	if err != nil {
		fmt.Println("Install session bus failed:", err)
		return
	}
	dbus.DealWithUnhandledMessage()

	C.start_loop()
}
