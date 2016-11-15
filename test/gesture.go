package main

import (
	"dbus/com/deepin/daemon/gesture"
	"fmt"
	"os/exec"
	"time"
)

const (
	gestureTypeSwipe = 100
	gestureTypePinch = 101

	gestureDirectionUp    = 10
	gestureDirectionDown  = 11
	gestureDirectionLeft  = 12
	gestureDirectionRight = 13
	gestureDirectionIn    = 14
	gestureDirectionOut   = 15
)

type gestureInfo struct {
	Name      int32
	Direction int32
	Fingers   int32
	Action    string
}
type gestureInfos []*gestureInfo

var handler = gestureInfos{
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionUp,
		Fingers:   3,
		Action:    "super+w",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionUp,
		Fingers:   4,
		Action:    "super+Up",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionDown,
		Fingers:   3,
		Action:    "super+d",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionDown,
		Fingers:   4,
		Action:    "super+Down",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionLeft,
		Fingers:   3,
		Action:    "alt+shift+Tab",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionLeft,
		Fingers:   4,
		Action:    "super+Left",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionRight,
		Fingers:   3,
		Action:    "alt+Tab",
	},
	{
		Name:      gestureTypeSwipe,
		Direction: gestureDirectionRight,
		Fingers:   4,
		Action:    "super+Right",
	},
	{
		Name:      gestureTypePinch,
		Direction: gestureDirectionIn,
		Fingers:   4,
		Action:    "super+s",
	},
	{
		Name:      gestureTypePinch,
		Direction: gestureDirectionOut,
		Fingers:   4,
		Action:    "super+s",
	},
}

func (infos gestureInfos) get(name, direction, fingers int32) *gestureInfo {
	for _, info := range infos {
		if info.Name == name &&
			info.Direction == direction &&
			info.Fingers == fingers {
			return info
		}
	}
	return nil
}

func main() {
	manager, err := gesture.NewGesture("com.deepin.daemon.Gesture",
		"/com/deepin/daemon/Gesture")
	if err != nil {
		fmt.Println("Failed to initialize gesture manager")
		return
	}

	manager.ConnectEvent(func(name, direction, fingers int32) {
		info := handler.get(name, direction, fingers)
		if info == nil {
			fmt.Println("No action found:", name, direction, fingers)
			return
		}
		out, err := exec.Command("/bin/sh", "-c",
			"exec xdotool key "+info.Action).CombinedOutput()
		if err != nil {
			fmt.Printf("Exec '%s' failed: %s\n", info.Action, string(out))
		}
	})

	for {
		time.Sleep(time.Second * 10)
	}
}
