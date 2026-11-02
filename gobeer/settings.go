package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"
)

// -------- SETTINGS, ADJUST FOR BETTER EXPERIENCE -------------
type GameSettings struct {
	screen_width  int32 //-w
	screen_height int32 //-h

	lengthIncPerBeer int32   //-i
	drunknessInc     float32 //fixd.

	sideMarginPixels   int32 //-ms
	topMarginPixels    int32 //-mt
	bottomMarginPixels int32 // -mb
	bottomFontSize     int32 //-fb
	topFontSize        int32 //-ft

	beerWidthPixels int32 //-b

	wormspeed    float32 //-ws
	wormturnstep float32 //-wt
	wormwidth    float32 //-ww

	drunknessFactor float32 //-d

	medkitVisibleTime  int64   //-kt How long time visible
	medkitProbpability float32 //-kp 1=100% on every beer drink
}

func defaultGameSettings() GameSettings {
	return GameSettings{
		screen_width:  1024,
		screen_height: 768,

		lengthIncPerBeer: 80,
		drunknessInc:     0.01,

		sideMarginPixels:   50,
		topMarginPixels:    100,
		bottomMarginPixels: 50,
		bottomFontSize:     40,
		topFontSize:        100,

		beerWidthPixels: 60,

		wormspeed:    1.85,
		wormturnstep: 0.04,
		wormwidth:    17,

		drunknessFactor: 6,

		medkitVisibleTime:  8000,
		medkitProbpability: 0.3,
	}
}

func parseI32(s string) (int32, error) {
	v, err := strconv.ParseInt(s, 10, 32)
	return int32(v), err
}

func parseF32(s string) (float32, error) {
	v, err := strconv.ParseFloat(s, 32)
	return float32(v), err
}

func getSettingsFromCmdline() (GameSettings, error) {
	result := defaultGameSettings()
	var err error

	for _, arg := range os.Args {
		fmt.Fprintf(os.Stderr, "%s\n", arg)
		if strings.HasPrefix(arg, "-w=") {
			if result.screen_width, err = parseI32(arg[3:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-h=") {
			if result.screen_height, err = parseI32(arg[3:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-i=") {
			if result.lengthIncPerBeer, err = parseI32(arg[3:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-ms=") {
			if result.sideMarginPixels, err = parseI32(arg[4:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-mt=") {
			if result.topMarginPixels, err = parseI32(arg[4:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-mb=") {
			if result.bottomMarginPixels, err = parseI32(arg[4:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-fb=") {
			if result.bottomFontSize, err = parseI32(arg[4:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-ft=") {
			if result.topFontSize, err = parseI32(arg[4:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-b=") {
			if result.beerWidthPixels, err = parseI32(arg[3:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-ws=") {
			if result.wormspeed, err = parseF32(arg[4:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-wt=") {
			if result.wormturnstep, err = parseF32(arg[4:]); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-ww=") {
			if result.wormwidth, err = parseF32(arg[4:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-d=") {
			if result.drunknessFactor, err = parseF32(arg[3:]); err != nil {
				return result, err
			}
		}

		if strings.HasPrefix(arg, "-kt=") {
			if result.medkitVisibleTime, err = strconv.ParseInt(arg[4:], 10, 64); err != nil {
				return result, err
			}
		}
		if strings.HasPrefix(arg, "-kp=") {
			if result.medkitProbpability, err = parseF32(arg[4:]); err != nil {
				return result, err
			}
		}
	}
	return result, nil
}
