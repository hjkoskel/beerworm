package main

import (
	_ "embed"
	"fmt"
	"math"
	"math/rand/v2"
	"os"
	"time"

	rl "github.com/gen2brain/raylib-go/raylib"
)

// Fixed seed, same as the zig version (deterministic sequence)
var rnd = rand.New(rand.NewPCG(0, 0))

//-----------------------------

func drunkDeltaAngle(d float32, factor float32) float32 {
	if d < 0.001 { //cutoff
		return 0
	}
	//TODO this needs adjustments for good gameplay
	return (rnd.Float32() - 0.5) * (1 - float32(math.Pow(math.E, float64(-d*factor))))
}

func vector2InArea(v rl.Vector2, area rl.Rectangle) bool {
	return (area.X < v.X) && (area.Y < v.Y) && (v.X < (area.X + area.Width)) && (v.Y < (area.Y + area.Height))
}

type Worm struct {
	elements        []rl.Vector2
	direction       rl.Vector2
	lenNow          int32
	speed           float32
	turnstep        float32
	width           float32
	drunkness       float32
	drunknessFactor float32
}

func (p *Worm) init(settings GameSettings) {
	p.elements = nil
	p.elements = append(p.elements, rl.Vector2{X: float32(settings.sideMarginPixels) * 2, Y: float32(settings.topMarginPixels) * 2})
	p.elements = append(p.elements, rl.Vector2{X: float32(settings.sideMarginPixels) * 2, Y: float32(settings.topMarginPixels) * 2})

	p.direction.X = 0
	p.direction.Y = 1.0
	p.lenNow = 100
	p.speed = settings.wormspeed
	p.turnstep = settings.wormturnstep
	p.width = settings.wormwidth
	p.drunkness = 0.00
	p.drunknessFactor = settings.drunknessFactor
}

func (p *Worm) draw() {
	lst := p.elements
	prevPoint := lst[0]

	var shade uint8 = 0
	for _, elem := range lst {
		rl.DrawLineEx(prevPoint, rl.Vector2Lerp(prevPoint, elem, 1.5), p.width, rl.Color{R: shade, B: 255, G: shade, A: 255})
		if 240 < shade {
			shade = 0
		}
		shade += 10
		prevPoint = elem
	}
}

func (p *Worm) move() {
	a := p.elements[len(p.elements)-1]
	p.elements = append(p.elements, rl.Vector2{X: a.X + p.direction.X, Y: a.Y + p.direction.Y})
	if int(p.lenNow) < len(p.elements) {
		p.elements = p.elements[1:]
	}
	//Add some drunk wobble
	p.turnAngle(drunkDeltaAngle(p.drunkness, p.drunknessFactor))
}

func (p *Worm) turnAngle(angle float32) {
	c := float32(math.Cos(float64(angle)))
	s := float32(math.Sin(float64(angle)))
	// NOTE: y uses the already updated x, same as the zig version
	p.direction.X = c*p.direction.X - s*p.direction.Y
	p.direction.Y = s*p.direction.X + c*p.direction.Y

	scale := p.speed / float32(math.Sqrt(float64(p.direction.X*p.direction.X+p.direction.Y*p.direction.Y)))
	p.direction.X *= scale
	p.direction.Y *= scale
}

func (p *Worm) turn(clockwise bool) {
	if clockwise {
		p.turnAngle(p.turnstep)
	} else {
		p.turnAngle(-p.turnstep)
	}
}

func (p *Worm) headInArea(area rl.Rectangle) bool {
	head := p.elements[len(p.elements)-1]
	return vector2InArea(head, area) //TODO enhance if wide worm is the thing
}

func (p *Worm) hitSelf() bool {
	lst := p.elements
	minDistanceSq := p.width * p.width

	head := lst[len(lst)-1]

	stillInHead := true

	for i := 1; i < len(lst); i++ {
		elem := lst[len(lst)-i]
		dSq := (elem.X-head.X)*(elem.X-head.X) + (elem.Y-head.Y)*(elem.Y-head.Y)
		if dSq < minDistanceSq {
			if !stillInHead {
				return true
			}
		} else {
			stillInHead = false
		}
	}
	return false
}

func getRandomVecInArea(a rl.Rectangle) rl.Vector2 {
	randX := rnd.Float32() * a.Width
	randY := rnd.Float32() * a.Height

	return rl.Vector2{X: a.X + randX, Y: a.Y + randY}
}

//go:embed gfx/olut_lidl.png
var bytImgLidl []byte

//go:embed gfx/olut_coop.png
var bytImgCoop []byte

//go:embed gfx/olut_heineken.png
var bytImgHeineken []byte

//go:embed gfx/olut_urquell.png
var bytImgUrquell []byte

//go:embed gfx/olut_alecoq.png
var bytImgAlecoq []byte

//go:embed gfx/medkit.png
var bytImgMedkit []byte

//go:embed snd/barney.mp3
var bytRoyhty []byte

//go:embed snd/doh.mp3
var bytDoh []byte

//go:embed snd/wohoo.mp3
var bytWohoo []byte

func loadImage(b []byte) *rl.Image {
	return rl.LoadImageFromMemory(".png", b, int32(len(b)))
}

func loadSound(b []byte) rl.Sound {
	return rl.LoadSoundFromWave(rl.LoadWaveFromMemory(".mp3", b, int32(len(b))))
}

func milliTimestamp() int64 {
	return time.Now().UnixMilli()
}

func main() {
	settings, err := getSettingsFromCmdline()
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}

	rl.InitWindow(settings.screen_width, settings.screen_height, "Kaljamato")
	rl.SetTargetFPS(60)
	rl.InitAudioDevice()
	defer rl.CloseWindow()

	var player Worm
	player.init(settings)

	beerTextures := []rl.Texture2D{
		rl.LoadTextureFromImage(loadImage(bytImgLidl)),
		rl.LoadTextureFromImage(loadImage(bytImgCoop)),
		rl.LoadTextureFromImage(loadImage(bytImgHeineken)),
		rl.LoadTextureFromImage(loadImage(bytImgUrquell)),
		rl.LoadTextureFromImage(loadImage(bytImgAlecoq)),
	}
	medkitTexture := rl.LoadTextureFromImage(loadImage(bytImgMedkit))

	snd := loadSound(bytRoyhty)
	gameOverSnd := loadSound(bytDoh)
	sndWohoo := loadSound(bytWohoo)

	beerPosition := rl.Vector2{X: float32(settings.screen_width) / 2, Y: float32(settings.screen_height) / 2.0}
	var score int32 = 0
	var maxDrunkness float32 = 0
	playarea := rl.Rectangle{
		X:      float32(settings.sideMarginPixels),
		Y:      float32(settings.topMarginPixels),
		Width:  float32(settings.screen_width - settings.sideMarginPixels*2),
		Height: float32(settings.screen_height - settings.bottomMarginPixels - settings.topMarginPixels),
	}
	beerIndexNow := rnd.IntN(len(beerTextures))

	medkitPosition := rl.Vector2{X: float32(settings.screen_width) / 2, Y: float32(settings.screen_height) / 2}
	var medkitAppeared int64 = 0 //In milliseconds.. unix epoch

	for !rl.WindowShouldClose() {
		//-------- INPUTS -----------
		if rl.IsKeyDown(rl.KeyRight) {
			player.turn(true)
		}
		if rl.IsKeyDown(rl.KeyLeft) {
			player.turn(false)
		}

		if rl.IsKeyReleased(rl.KeyP) {
			fmt.Fprintf(os.Stderr, "PAUSE down\n")
			rl.PollInputEvents()
			for !rl.IsKeyReleased(rl.KeyP) {
				rl.PollInputEvents()
				rl.WaitTime(0.1)
			}
			fmt.Fprintf(os.Stderr, "PAUSE up\n")
		}
		if rl.IsKeyReleased(rl.KeyF) {
			rl.ToggleFullscreen()
		}
		//----- RENDER -------
		rl.BeginDrawing()
		rl.DrawRectangleGradientEx(rl.Rectangle{X: 0, Y: 0, Width: float32(settings.screen_width), Height: float32(settings.screen_height)}, rl.Beige, rl.Black, rl.Yellow, rl.Red)
		rl.DrawRectangleRec(playarea, rl.Black)

		rl.DrawText("Kaljamato", settings.screen_width/6, 0, settings.topFontSize, rl.Blue)

		rl.DrawText(fmt.Sprintf("Alc: %02.2f Score: %d", player.drunkness, score), settings.screen_width/3, settings.screen_height-settings.bottomMarginPixels+(settings.bottomMarginPixels-settings.bottomFontSize)/2, settings.bottomFontSize, rl.Beige)

		beerScale := float32(settings.beerWidthPixels) / float32(beerTextures[beerIndexNow].Width)
		rl.DrawTextureEx(beerTextures[beerIndexNow], beerPosition, 0, beerScale, rl.White)

		//is medkit visible
		medkitVisible := (milliTimestamp() - medkitAppeared) < settings.medkitVisibleTime
		medkitScale := float32(settings.beerWidthPixels) / float32(medkitTexture.Width)
		if medkitVisible {
			rl.DrawTextureEx(medkitTexture, medkitPosition, 0, medkitScale, rl.White)
		}
		player.draw()
		rl.EndDrawing()

		player.move()

		beerheight := beerScale * float32(beerTextures[beerIndexNow].Height)
		medkitheight := medkitScale * float32(medkitTexture.Height)

		if medkitVisible && player.headInArea(rl.Rectangle{X: medkitPosition.X, Y: medkitPosition.Y, Width: float32(settings.beerWidthPixels), Height: medkitheight}) {
			player.drunkness = 0
			medkitAppeared = 0
			rl.PlaySound(sndWohoo)
		}

		if player.headInArea(rl.Rectangle{X: beerPosition.X, Y: beerPosition.Y, Width: float32(settings.beerWidthPixels), Height: beerheight}) {
			score += 1
			rl.PlaySound(snd)
			player.lenNow += settings.lengthIncPerBeer
			player.drunkness += settings.drunknessInc

			if maxDrunkness < player.drunkness {
				maxDrunkness = player.drunkness
			}

			beerRandArea := rl.Rectangle{X: playarea.X, Y: playarea.Y, Width: playarea.Width - float32(settings.beerWidthPixels), Height: playarea.Height - beerheight}

			if rnd.Float32() < settings.medkitProbpability {
				medkitPosition = getRandomVecInArea(beerRandArea)
				medkitAppeared = milliTimestamp()
			}

			beerPosition = getRandomVecInArea(beerRandArea)
			beerIndexNow = rnd.IntN(len(beerTextures))
		}
		if player.hitSelf() {
			fmt.Fprintf(os.Stderr, "Hit to self! \n")
			break
		}
		if !player.headInArea(playarea) {
			fmt.Fprintf(os.Stderr, "Out from game area! \n")
			break
		}
	}
	//GAME OVER
	rl.PlaySound(gameOverSnd)
	rl.WaitTime(0.1)
	rl.BeginDrawing()
	rl.ClearBackground(rl.Black)
	rl.DrawText(fmt.Sprintf("GAME OVER, max drunkness %02.2f Score: %d", maxDrunkness, score), 10, 10, 20, rl.White)
	rl.EndDrawing()
	for rl.GetKeyPressed() == 0 {
		rl.PollInputEvents()
		rl.WaitTime(0.1)
	}
}
