// src/main.zig
const r = @cImport({
    @cInclude("raylib.h");
    @cInclude("raymath.h");
});

const std = @import("std");
const m = std.math;
const RndGen = std.rand.DefaultPrng;
var rnd = RndGen.init(0);

var gpa_server = std.heap.GeneralPurposeAllocator(.{ .stack_trace_frames = 12 }){};
const alloc = gpa_server.allocator();

// -------- SETTINGS, ADJUST FOR BETTER EXPERIENCE -------------
const GameSettings = struct {
    screen_width: i32 = 1024, //-w
    screen_height: i32 = 768, //-h

    lengthIncPerBeer: i32 = 80, //-i
    drunknessInc: f32 = 0.01, //fixd.

    sideMarginPixels: i32 = 50, //-ms
    topMarginPixels: i32 = 100, //-mt
    bottomMarginPixels: i32 = 50, // -mb
    bottomFontSize: i32 = 40, //-fb
    topFontSize: i32 = 100, //-ft

    beerWidthPixels: i32 = 60, //-b

    wormspeed: f32 = 1.85, //-ws
    wormturnstep: f32 = 0.04, //-wt
    wormwidth: f32 = 17, //-ww

    drunknessFactor: f32 = 6, //-d

    medkitVisibleTime: i64 = 8000, //-kt How long time visible
    medkitProbpability: f32 = 0.3, //-kp 1=100% on every beer drink
};

fn getSettingsFromCmdline() !GameSettings {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    var args = try std.process.argsWithAllocator(allocator);
    defer args.deinit();
    var result: GameSettings = .{};

    while (args.next()) |arg| {
        std.debug.print("{s}\n", .{arg});
        if (std.mem.startsWith(u8, arg, "-w=")) {
            result.screen_width = try std.fmt.parseInt(i32, arg[3..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-h=")) {
            result.screen_height = try std.fmt.parseInt(i32, arg[3..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-i=")) {
            result.lengthIncPerBeer = try std.fmt.parseInt(i32, arg[3..], 10);
        }

        if (std.mem.startsWith(u8, arg, "-ms=")) {
            result.sideMarginPixels = try std.fmt.parseInt(i32, arg[4..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-mt=")) {
            result.topMarginPixels = try std.fmt.parseInt(i32, arg[4..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-mb=")) {
            result.bottomMarginPixels = try std.fmt.parseInt(i32, arg[4..], 10);
        }

        if (std.mem.startsWith(u8, arg, "-fb=")) {
            result.bottomFontSize = try std.fmt.parseInt(i32, arg[4..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-ft=")) {
            result.topFontSize = try std.fmt.parseInt(i32, arg[4..], 10);
        }

        if (std.mem.startsWith(u8, arg, "-b=")) {
            result.beerWidthPixels = try std.fmt.parseInt(i32, arg[3..], 10);
        }

        if (std.mem.startsWith(u8, arg, "-ws=")) {
            result.wormspeed = try std.fmt.parseFloat(f32, arg[4..]);
        }
        if (std.mem.startsWith(u8, arg, "-wt=")) {
            result.wormturnstep = try std.fmt.parseFloat(f32, arg[4..]);
        }
        if (std.mem.startsWith(u8, arg, "-ww=")) {
            result.wormwidth = try std.fmt.parseFloat(f32, arg[4..]);
        }

        if (std.mem.startsWith(u8, arg, "-d=")) {
            result.drunknessFactor = try std.fmt.parseFloat(f32, arg[3..]);
        }

        if (std.mem.startsWith(u8, arg, "-kt=")) {
            result.medkitVisibleTime = try std.fmt.parseInt(i64, arg[3..], 10);
        }
        if (std.mem.startsWith(u8, arg, "-kp=")) {
            result.medkitProbpability = try std.fmt.parseFloat(f32, arg[4..]);
        }
    }
    return result;
}

//-----------------------------

fn drunkDeltaAngle(d: f32, factor: f32) f32 {
    if (d < 0.001) { //cutoff
        return 0;
    }
    //TODO this needs adjustments for good gameplay
    return (rnd.random().float(f32) - 0.5) * (1 - m.pow(f32, std.math.e, -d * factor));
}

fn vector2InArea(v: r.Vector2, area: r.Rectangle) bool {
    return (area.x < v.x) and (area.y < v.y) and (v.x < (area.x + area.width)) and (v.y < (area.y + area.height));
}

const Worm = struct {
    elements: std.ArrayList(r.Vector2),
    direction: r.Vector2,
    lenNow: i32,
    speed: f32,
    turnstep: f32,
    width: f32,
    drunkness: f32,
    drunknessFactor: f32,

    pub fn init(p: *Worm, settings: GameSettings) !void {
        p.elements = std.ArrayList(r.Vector2).init(alloc);
        try p.elements.append(.{ .x = @as(f32, @floatFromInt(settings.sideMarginPixels)) * 2, .y = @as(f32, @floatFromInt(settings.topMarginPixels)) * 2 });
        try p.elements.append(.{ .x = @as(f32, @floatFromInt(settings.sideMarginPixels)) * 2, .y = @as(f32, @floatFromInt(settings.topMarginPixels)) * 2 });

        p.direction.x = 0;
        p.direction.y = 1.0;
        p.lenNow = 100;
        p.speed = settings.wormspeed;
        p.turnstep = settings.wormturnstep;
        p.width = settings.wormwidth;
        p.drunkness = 0.00;
        p.drunknessFactor = settings.drunknessFactor;
    }

    pub fn draw(p: *Worm) void {
        const lst = p.elements.items;
        var prevPoint = lst[0];

        var shade: u8 = 0;
        for (lst) |elem| {
            r.DrawLineEx(prevPoint, r.Vector2Lerp(prevPoint, elem, 1.5), p.width, r.Color{ .r = shade, .b = 255, .g = shade, .a = 255 });
            if (240 < shade) {
                shade = 0;
            }
            shade += 10;
            prevPoint = elem;
        }
    }
    pub fn move(p: *Worm) !void {
        const a = p.elements.getLast();
        try p.elements.append(.{ .x = a.x + p.direction.x, .y = a.y + p.direction.y });
        if (p.lenNow < p.elements.items.len) {
            _ = p.elements.orderedRemove(0);
        }
        //Add some drunk wobble
        p.turnAngle(drunkDeltaAngle(p.drunkness, p.drunknessFactor));
    }

    pub fn turnAngle(p: *Worm, angle: f32) void {
        p.direction.x = m.cos(angle) * p.direction.x - m.sin(angle) * p.direction.y;
        p.direction.y = m.sin(angle) * p.direction.x + m.cos(angle) * p.direction.y;

        const scale = p.speed / m.sqrt(p.direction.x * p.direction.x + p.direction.y * p.direction.y);
        p.direction.x *= scale;
        p.direction.y *= scale;
    }
    pub fn turn(p: *Worm, clockwise: bool) void {
        if (clockwise) {
            p.turnAngle(p.turnstep);
        } else {
            p.turnAngle(-p.turnstep);
        }
    }

    pub fn headInArea(p: *Worm, area: r.Rectangle) bool {
        const head = p.elements.getLast();
        return vector2InArea(head, area); //TODO enhance if wide worm is the thing
    }

    pub fn hitSelf(p: *Worm) bool {
        const lst = p.elements.items;
        const minDistanceSq = p.width * p.width;

        const head = lst[lst.len - 1];

        var stillInHead: bool = true;

        for (1..lst.len) |i| {
            const elem = lst[lst.len - i];
            const dSq = (elem.x - head.x) * (elem.x - head.x) + (elem.y - head.y) * (elem.y - head.y);
            //std.debug.print("dSq= {d:.3} \n", .{dSq});
            if (dSq < minDistanceSq) {
                if (!stillInHead) {
                    return true;
                }
            } else {
                stillInHead = false;
            }
        }
        return false;
    }
};

fn getRandomVecInArea(a: r.Rectangle) r.Vector2 {
    const randX: f32 = rnd.random().float(f32) * a.width;
    const randY: f32 = rnd.random().float(f32) * a.height;

    const result: r.Vector2 = .{ .x = a.x + randX, .y = a.y + randY };
    return result;
}

const bytImgLidl = @embedFile("./gfx/olut_lidl.png");
const bytImgCoop = @embedFile("./gfx/olut_coop.png");
const bytImgHeineken = @embedFile("./gfx/olut_heineken.png");
const bytImgUrquell = @embedFile("./gfx/olut_urquell.png");
const bytImgAlecoq = @embedFile("./gfx/olut_alecoq.png");

const bytImgMedkit = @embedFile("./gfx/medkit.png");

const bytRoyhty = @embedFile("./snd/barney.mp3");
const bytDoh = @embedFile("./snd/doh.mp3");
const bytWohoo = @embedFile("./snd/wohoo.mp3");

pub fn main() !void {
    //var settings: GameSettings = .{};
    const settings = try getSettingsFromCmdline();

    r.InitWindow(settings.screen_width, settings.screen_height, "Kaljamato");
    r.SetTargetFPS(60);
    r.InitAudioDevice();
    defer r.CloseWindow();

    var player: Worm = undefined;
    try player.init(settings);

    const imgLidl = r.LoadImageFromMemory(".png", bytImgLidl, bytImgLidl.len);
    const imgCoop = r.LoadImageFromMemory(".png", bytImgCoop, bytImgCoop.len);
    const imgHeineken = r.LoadImageFromMemory(".png", bytImgHeineken, bytImgHeineken.len);
    const imgUrquell = r.LoadImageFromMemory(".png", bytImgUrquell, bytImgUrquell.len);
    const imgAlecoq = r.LoadImageFromMemory(".png", bytImgAlecoq, bytImgAlecoq.len);

    const beerTextures = [_]r.Texture{ r.LoadTextureFromImage(imgLidl), r.LoadTextureFromImage(imgCoop), r.LoadTextureFromImage(imgHeineken), r.LoadTextureFromImage(imgUrquell), r.LoadTextureFromImage(imgAlecoq) };
    const medkitTexture = r.LoadTextureFromImage(r.LoadImageFromMemory(".png", bytImgMedkit, bytImgMedkit.len));

    const snd = r.LoadSoundFromWave(r.LoadWaveFromMemory(".mp3", bytRoyhty, bytRoyhty.len));
    const gameOverSnd = r.LoadSoundFromWave(r.LoadWaveFromMemory(".mp3", bytDoh, bytDoh.len));
    const sndWohoo = r.LoadSoundFromWave(r.LoadWaveFromMemory(".mp3", bytWohoo, bytWohoo.len));

    var beerPosition: r.Vector2 = .{ .x = @as(f32, @floatFromInt(settings.screen_width)) / 2, .y = @as(f32, @floatFromInt(settings.screen_height)) / 2.0 };
    var score: i32 = 0;
    var maxDrunkness: f32 = 0;
    const playarea = r.Rectangle{ .x = @as(f32, @floatFromInt(settings.sideMarginPixels)), .y = @as(f32, @floatFromInt(settings.topMarginPixels)), .width = @floatFromInt(settings.screen_width - settings.sideMarginPixels * 2), .height = @floatFromInt(settings.screen_height - settings.bottomMarginPixels - settings.topMarginPixels) };
    var beerIndexNow: usize = rnd.random().intRangeAtMost(usize, 0, beerTextures.len - 1);

    var medkitPosition: r.Vector2 = .{ .x = @as(f32, @floatFromInt(settings.screen_width)) / 2, .y = @as(f32, @floatFromInt(settings.screen_height)) / 2 };
    var medkitAppeared: i64 = 0; //In milliseconds.. unix epoch

    while (!r.WindowShouldClose()) {
        //-------- INPUTS -----------
        if (r.IsKeyDown(r.KEY_RIGHT)) {
            player.turn(true);
        }
        if (r.IsKeyDown(r.KEY_LEFT)) {
            player.turn(false);
        }

        if (r.IsKeyReleased(r.KEY_P)) {
            std.debug.print("PAUSE down\n", .{});
            r.PollInputEvents();
            while (!r.IsKeyReleased(r.KEY_P)) {
                r.PollInputEvents();
                r.WaitTime(0.1);
            }
            std.debug.print("PAUSE up\n", .{});
        }
        if (r.IsKeyReleased(r.KEY_F)) {
            r.ToggleFullscreen();
        }
        //----- RENDER -------
        r.BeginDrawing();
        r.DrawRectangleGradientEx(.{ .x = 0, .y = 0, .width = @floatFromInt(settings.screen_width), .height = @floatFromInt(settings.screen_height) }, r.BEIGE, r.BLACK, r.YELLOW, r.RED);
        r.DrawRectangleRec(playarea, r.BLACK);

        r.DrawText("Kaljamato", @divFloor(settings.screen_width, 6), 0, settings.topFontSize, r.BLUE);
        //r.DrawText("Kaljamato", 0, 0, settings.topFontSize, r.BLUE);

        r.DrawText(r.TextFormat("Alc: %02.02f Score: %i", player.drunkness, score), @divFloor(settings.screen_width, 3), settings.screen_height - settings.bottomMarginPixels + @divFloor((settings.bottomMarginPixels - settings.bottomFontSize), 2), settings.bottomFontSize, r.BEIGE);

        const beerScale: f32 = @as(f32, @floatFromInt(settings.beerWidthPixels)) / @as(f32, @floatFromInt(beerTextures[beerIndexNow].width));
        r.DrawTextureEx(beerTextures[beerIndexNow], beerPosition, 0, beerScale, r.WHITE);

        //is medkit visible
        const medkitVisible = (std.time.milliTimestamp() - medkitAppeared) < settings.medkitVisibleTime;
        const medkitScale = @as(f32, @floatFromInt(settings.beerWidthPixels)) / @as(f32, @floatFromInt(medkitTexture.width));
        if (medkitVisible) {
            r.DrawTextureEx(medkitTexture, medkitPosition, 0, medkitScale, r.WHITE);
        }
        player.draw();
        r.EndDrawing();

        try player.move();

        var beerheight = beerScale * @as(f32, @floatFromInt(beerTextures[beerIndexNow].height));

        var medkitheight = medkitScale * @as(f32, @floatFromInt(medkitTexture.height));
        if (medkitVisible and (player.headInArea(.{ .x = medkitPosition.x, .y = medkitPosition.y, .width = @as(f32, @floatFromInt(settings.beerWidthPixels)), .height = medkitheight }))) {
            player.drunkness = 0;
            medkitAppeared = 0;
            r.PlaySound(sndWohoo);
        }

        if (player.headInArea(.{ .x = beerPosition.x, .y = beerPosition.y, .width = @as(f32, @floatFromInt(settings.beerWidthPixels)), .height = beerheight })) {
            score += 1;
            r.PlaySound(snd);
            player.lenNow += settings.lengthIncPerBeer;
            player.drunkness += settings.drunknessInc;

            if (maxDrunkness < player.drunkness) {
                maxDrunkness = player.drunkness;
            }

            const beerRandArea = r.Rectangle{ .x = playarea.x, .y = playarea.y, .width = playarea.width - @as(f32, @floatFromInt(settings.beerWidthPixels)), .height = playarea.height - beerheight };

            if (rnd.random().float(f32) < settings.medkitProbpability) {
                medkitPosition = getRandomVecInArea(beerRandArea);
                medkitAppeared = std.time.milliTimestamp();
            }

            beerPosition = getRandomVecInArea(beerRandArea);
            beerIndexNow = rnd.random().intRangeAtMost(usize, 0, beerTextures.len - 1);
        }
        if (player.hitSelf()) {
            std.debug.print("Hit to self! \n", .{});
            break;
        }
        if (!player.headInArea(playarea)) {
            std.debug.print("Out from game area! \n", .{});
            break;
        }
    }
    //GAME OVER
    r.PlaySound(gameOverSnd);
    r.WaitTime(0.1);
    r.BeginDrawing();
    r.ClearBackground(r.BLACK);
    r.DrawText(r.TextFormat("GAME OVER, max drunkness %02.02f Score: %i", maxDrunkness, score), 10, 10, 20, r.WHITE);
    r.EndDrawing();
    while (r.GetKeyPressed() == 0) {
        r.PollInputEvents();
        r.WaitTime(0.1);
    }
}
