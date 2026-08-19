#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "map_monitor.h"
#include "map_tile_library.h"
#include "fe8_unit_visuals.h"

namespace
{
constexpr CGFloat kMargin = 24.0;
constexpr CGFloat kSidebarWidth = 270.0;

struct PaletteColor
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};
using Palette = std::array<PaletteColor, 16>;

std::string fe8_asset_path(const std::string& root, const std::string_view relative_path)
{
    return root + "/assets/fe8/" + std::string(relative_path);
}

Palette load_gba_palette(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open map-unit palette: " + path);
    }

    Palette result{};
    for (PaletteColor& color : result)
    {
        unsigned char low = 0;
        unsigned char high = 0;
        file.read(reinterpret_cast<char*>(&low), 1);
        file.read(reinterpret_cast<char*>(&high), 1);
        if (!file)
        {
            throw std::runtime_error("Map-unit palette is incomplete: " + path);
        }
        const std::uint16_t bgr555 = static_cast<std::uint16_t>(low) |
                                     (static_cast<std::uint16_t>(high) << 8);
        color.r = static_cast<std::uint8_t>(((bgr555 >> 0) & 31) * 255 / 31);
        color.g = static_cast<std::uint8_t>(((bgr555 >> 5) & 31) * 255 / 31);
        color.b = static_cast<std::uint8_t>(((bgr555 >> 10) & 31) * 255 / 31);
    }
    return result;
}

int palette_distance(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                     const PaletteColor& color)
{
    const int dr = static_cast<int>(r) - color.r;
    const int dg = static_cast<int>(g) - color.g;
    const int db = static_cast<int>(b) - color.b;
    return dr * dr + dg * dg + db * db;
}

NSImage* load_image(const std::string& path)
{
    return [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]];
}

// The FE8 text-engine atlas is an indexed PNG with an opaque white
// background. Turn its black glyph pixels into a transparent, tinted image
// once at startup.  This keeps the forecast's lettering pixel-perfect at any
// monitor scale instead of falling back to macOS's vector system fonts.
NSImage* tint_fe8_font_atlas(NSImage* source, NSColor* tint,
                             std::array<int, 128>* glyph_widths = nullptr)
{
    if (source == nil)
    {
        return nil;
    }

    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }

    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);
    std::vector<std::uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }

    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);

    CGFloat red = 0.0;
    CGFloat green = 0.0;
    CGFloat blue = 0.0;
    CGFloat alpha = 0.0;
    // `colorWithCalibratedWhite:` is grayscale, so AppKit requires an
    // explicit conversion before its red/green/blue components are queried.
    NSColor* rgb_tint = [tint colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
    if (rgb_tint == nil)
    {
        rgb_tint = [NSColor colorWithDeviceRed:0.0 green:0.0 blue:0.0 alpha:1.0];
    }
    [rgb_tint getRed:&red green:&green blue:&blue alpha:&alpha];

    std::array<int, 128> left{};
    std::array<int, 128> right{};
    left.fill(16);
    right.fill(-1);

    for (std::size_t y = 0; y < height; ++y)
    {
        for (std::size_t x = 0; x < width; ++x)
        {
            std::uint8_t* rgba = pixels.data() + (y * width + x) * 4;
            // The source glyph ink is dark; its paper-white backdrop is not
            // part of the real GBA font and must become transparent.
            const bool ink = rgba[3] != 0 &&
                (static_cast<int>(rgba[0]) + static_cast<int>(rgba[1]) +
                 static_cast<int>(rgba[2])) < 480;

            if (!ink)
            {
                rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
                continue;
            }

            rgba[0] = static_cast<std::uint8_t>(red * 255.0);
            rgba[1] = static_cast<std::uint8_t>(green * 255.0);
            rgba[2] = static_cast<std::uint8_t>(blue * 255.0);
            rgba[3] = static_cast<std::uint8_t>(alpha * 255.0);

            if (glyph_widths != nullptr)
            {
                const int code = static_cast<int>((y / 16) * 16 + (x / 16));
                if (code >= 0 && code < static_cast<int>(glyph_widths->size()))
                {
                    const int local_x = static_cast<int>(x % 16);
                    left[code] = std::min(left[code], local_x);
                    right[code] = std::max(right[code], local_x);
                }
            }
        }
    }

    if (glyph_widths != nullptr)
    {
        for (std::size_t code = 0; code < glyph_widths->size(); ++code)
        {
            (*glyph_widths)[code] = right[code] >= left[code]
                ? std::min(16, right[code] + 2)
                : 4;
        }
        (*glyph_widths)[static_cast<unsigned char>(' ')] = 4;
    }

    CGImageRef fixed = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:fixed size:source.size];
    CGImageRelease(fixed);
    return result;
}

void draw_fe8_bitmap_text(NSImage* atlas, const std::array<int, 128>& widths,
                          NSString* text, NSPoint origin, CGFloat scale)
{
    if (atlas == nil || text == nil)
    {
        return;
    }

    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    CGFloat x = origin.x;
    for (NSUInteger index = 0; index < text.length; ++index)
    {
        unichar character = [text characterAtIndex:index];
        if (character >= widths.size())
        {
            character = '?';
        }

        const NSRect source = NSMakeRect(
            static_cast<CGFloat>((character & 15) * 16),
            // The FE8 atlas indexes character row 0 from its top edge, while
            // NSImage source rectangles use a bottom-left origin.
            static_cast<CGFloat>((15 - (character >> 4)) * 16), 16.0, 16.0
        );
        const NSRect target = NSMakeRect(x, origin.y, 16.0 * scale, 16.0 * scale);
        [atlas drawInRect:target
                 fromRect:source
                operation:NSCompositingOperationSourceOver
                 fraction:1.0
           respectFlipped:YES
                    hints:nil];
        x += static_cast<CGFloat>(widths[character]) * scale;
    }
}

void draw_fe8_outlined_text(NSImage* white_atlas, NSImage* ink_atlas,
                            const std::array<int, 128>& widths,
                            NSString* text, NSPoint origin, CGFloat scale)
{
    const CGFloat pixel = std::max<CGFloat>(1.0, std::round(scale));
    for (const std::pair<int, int>& offset : {
             std::pair{-1, -1}, std::pair{0, -1}, std::pair{1, -1},
             std::pair{-1,  0},                     std::pair{1,  0},
             std::pair{-1,  1}, std::pair{0,  1}, std::pair{1,  1}
         })
    {
        draw_fe8_bitmap_text(
            ink_atlas, widths, text,
            NSMakePoint(origin.x + offset.first * pixel,
                        origin.y + offset.second * pixel),
            scale
        );
    }
    draw_fe8_bitmap_text(white_atlas, widths, text, origin, scale);
}

// The original GBA palette's index 0 is transparent. The extracted sheets
// retain it as opaque PNG pixels, so restore alpha and map the source's
// palette indices to the requested team palette before a unit is drawn.
NSImage* restore_sprite_transparency(NSImage* source,
                                    const Palette& source_palette,
                                    const Palette& output_palette)
{
    if (source == nil)
    {
        return nil;
    }
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }

    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);
    std::vector<std::uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);

    for (std::size_t index = 0; index < width * height; ++index)
    {
        std::uint8_t* rgba = pixels.data() + index * 4;
        int closest = 0;
        int distance = palette_distance(rgba[0], rgba[1], rgba[2], source_palette[0]);
        for (int entry = 1; entry < 16; ++entry)
        {
            const int candidate = palette_distance(
                rgba[0], rgba[1], rgba[2], source_palette[entry]
            );
            if (candidate < distance)
            {
                closest = entry;
                distance = candidate;
            }
        }
        if (closest == 0)
        {
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
        }
        else
        {
            rgba[0] = output_palette[closest].r;
            rgba[1] = output_palette[closest].g;
            rgba[2] = output_palette[closest].b;
            rgba[3] = 255;
        }
    }

    CGImageRef fixed = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:fixed size:source.size];
    CGImageRelease(fixed);
    return result;
}

Palette custom_team_palette(const Palette& player, std::uint32_t rgb)
{
    Palette result = player;
    const std::uint8_t red = static_cast<std::uint8_t>((rgb >> 16) & 0xFF);
    const std::uint8_t green = static_cast<std::uint8_t>((rgb >> 8) & 0xFF);
    const std::uint8_t blue = static_cast<std::uint8_t>(rgb & 0xFF);

    // These are the exact entries altered by FE8's Player/Enemy/NPC/P4
    // palettes. Every other entry (skin, steel, white, and transparency)
    // remains untouched. The player-palette brightness supplies the shades.
    for (const int entry : {1, 2, 3, 7, 8, 9, 10, 11})
    {
        const int brightness = std::max({
            static_cast<int>(player[entry].r),
            static_cast<int>(player[entry].g),
            static_cast<int>(player[entry].b)
        });
        result[entry] = PaletteColor{
            static_cast<std::uint8_t>((static_cast<int>(red) * brightness + 127) / 255),
            static_cast<std::uint8_t>((static_cast<int>(green) * brightness + 127) / 255),
            static_cast<std::uint8_t>((static_cast<int>(blue) * brightness + 127) / 255)
        };
    }
    return result;
}

std::uint32_t palette_key(const fe_tiles::GuildColor& color)
{
    return (static_cast<std::uint32_t>(color.scheme) << 24) |
           (color.rgb & 0x00FFFFFF);
}

NSImage* white_sprite(NSImage* source)
{
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }
    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);
    std::vector<std::uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);
    for (std::size_t index = 0; index < width * height; ++index)
    {
        std::uint8_t* rgba = pixels.data() + index * 4;
        if (rgba[3] != 0)
        {
            rgba[0] = rgba[1] = rgba[2] = 255;
        }
    }
    CGImageRef white = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:white size:source.size];
    CGImageRelease(white);
    return result;
}

// FE's acted-unit state is a palette effect. We preserve the alpha and
// sprite geometry, then desaturate the renderer's already-team-coloured art.
NSImage* greyscale_sprite(NSImage* source)
{
    NSRect proposed = NSMakeRect(0.0, 0.0, source.size.width, source.size.height);
    CGImageRef image = [source CGImageForProposedRect:&proposed context:nil hints:nil];
    if (image == nullptr)
    {
        return nil;
    }
    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);
    std::vector<std::uint8_t> pixels(width * height * 4, 0);
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(), width, height, 8, width * 4, colors,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colors);
    if (context == nullptr)
    {
        return nil;
    }
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);
    for (std::size_t index = 0; index < width * height; ++index)
    {
        std::uint8_t* rgba = pixels.data() + index * 4;
        if (rgba[3] != 0)
        {
            const std::uint8_t shade = static_cast<std::uint8_t>(
                (static_cast<unsigned>(rgba[0]) * 30U +
                 static_cast<unsigned>(rgba[1]) * 59U +
                 static_cast<unsigned>(rgba[2]) * 11U) / 100U
            );
            rgba[0] = shade;
            rgba[1] = shade;
            rgba[2] = shade;
        }
    }
    CGImageRef greyscale = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    NSImage* result = [[NSImage alloc] initWithCGImage:greyscale size:source.size];
    CGImageRelease(greyscale);
    return result;
}

NSImage* canvas_image(const fe_tiles::TileCanvas& canvas)
{
    unsigned char* pixels = const_cast<unsigned char*>(canvas.rgba().data());
    NSBitmapImageRep* representation = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:&pixels
        pixelsWide:canvas.pixel_width()
        pixelsHigh:canvas.pixel_height()
        bitsPerSample:8
        samplesPerPixel:4
        hasAlpha:YES
        isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:NSBitmapFormatThirtyTwoBitBigEndian
        bytesPerRow:canvas.pixel_width() * 4
        bitsPerPixel:32];
    if (representation == nil)
    {
        return nil;
    }
    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(
        canvas.pixel_width(), canvas.pixel_height()
    )];
    [image addRepresentation:representation];
    return image;
}

void draw_text(NSString* text, NSPoint origin, NSFont* font, NSColor* color)
{
    [text drawAtPoint:origin withAttributes:@{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: color,
    }];
}

void draw_outlined_text(NSString* text, NSPoint origin, NSFont* font,
                        NSColor* fill, CGFloat outline = 1.0)
{
    NSColor* edge = [NSColor colorWithCalibratedWhite:0.04 alpha:1.0];
    draw_text(text, NSMakePoint(origin.x - outline, origin.y), font, edge);
    draw_text(text, NSMakePoint(origin.x + outline, origin.y), font, edge);
    draw_text(text, NSMakePoint(origin.x, origin.y - outline), font, edge);
    draw_text(text, NSMakePoint(origin.x, origin.y + outline), font, edge);
    draw_text(text, origin, font, fill);
}

ItemID equipped_item(const Entity& unit)
{
    const int slot = unit.inventory.EquippedSlot;
    if (slot < 0 || slot >= 5)
    {
        return NO_ITEM;
    }
    return unit.inventory.slot[slot].ID;
}

std::string item_name_for_forecast(ItemID item)
{
    switch (item)
    {
        case IRON_SWORD: return "Iron Sword";
        case IRON_LANCE: return "Iron Lance";
        case IRON_AXE: return "Iron Axe";
        case IRON_BOW: return "Iron Bow";
        case FIRE: return "Fire";
        case LIGHTNING: return "Lightning";
        case FLUX: return "Flux";
        case THUNDER: return "Thunder";
        default: return "--";
    }
}

std::string_view item_icon_for_forecast(ItemID item)
{
    switch (item)
    {
        case IRON_SWORD: return "graphics/item_icon/item_icon_sword_iron.png";
        case IRON_LANCE: return "graphics/item_icon/item_icon_lance_iron.png";
        case IRON_AXE: return "graphics/item_icon/item_icon_axe_iron.png";
        case IRON_BOW: return "graphics/item_icon/item_icon_bow_iron.png";
        case FIRE: return "graphics/item_icon/item_icon_anima_fire.png";
        case LIGHTNING: return "graphics/item_icon/item_icon_light_lightning.png";
        case FLUX: return "graphics/item_icon/item_icon_dark_flux.png";
        case THUNDER: return "graphics/item_icon/item_icon_anima_thunder.png";
        default: return {};
    }
}
}

namespace fe_tiles
{
struct MapMonitor::Impl
{
    struct PhaseIntro
    {
        std::string guild_name;
        GuildColor color = GuildColor::player();
        int tick = 0;
    };

    struct PhaseDialogue
    {
        std::string guild_name;
        GuildColor color = GuildColor::player();
    };

    struct GameOver
    {
        std::uint64_t tick = 0;
    };

    struct BattleForecast
    {
        std::string attacker_name;
        std::string defender_name;
        int attacker_hp = 0;
        int attacker_max_hp = 0;
        int defender_hp = 0;
        int defender_max_hp = 0;
        ItemID attacker_weapon = NO_ITEM;
        ItemID defender_weapon = NO_ITEM;
        CombatInfo attacker_combat{};
        CombatInfo defender_combat{};
    };

    maps::MapRecipe recipe;
    AnimationRenderer* renderer = nullptr;
    Options options;
    std::unique_ptr<TileCanvas> canvas;
    NSImage* image = nil;
    // Literal 32x8 FE8 map-animation glyph extracted from Img_MapAnimMISS.
    // It is independent of a unit sheet or team palette.
    NSImage* miss_image = nil;
    NSWindow* window = nil;
    NSView* view = nil;
    NSTimer* timer = nil;
    std::function<void(char)> key_callback;
    std::function<void()> frame_callback;
    std::optional<std::pair<int, int>> cursor;
    std::vector<std::vector<std::pair<int, int>>> route_arrows;
    std::optional<BattleForecast> battle_forecast;
    std::optional<PhaseIntro> phase_intro;
    std::optional<PhaseDialogue> phase_dialogue;
    std::optional<GameOver> game_over;
    double battle_tick_accumulator = 0.0;
    std::uint64_t idle_tick = 0;
    Palette player_palette{};
    Palette enemy_palette{};
    Palette npc_palette{};
    Palette fourth_palette{};
    NSMutableDictionary<NSString*, NSImage*>* raw_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* raw_wait_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* wait_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* white_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* greyscale_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* greyscale_wait_sheets = nil;
    NSMutableDictionary<NSString*, NSImage*>* item_icons = nil;
    NSMutableDictionary<NSString*, NSNumber*>* opaque_tops = nil;
    NSImage* forecast_font_ink = nil;
    NSImage* forecast_font_white = nil;
    std::array<int, 128> forecast_glyph_widths{};

    Impl(maps::MapRecipe next_recipe, AnimationRenderer& next_renderer, Options next_options)
        : recipe(std::move(next_recipe)), renderer(&next_renderer), options(std::move(next_options))
    {
        rebuild_canvas();
        load_unit_sheets();
    }

    void require_compatible(const maps::MapRecipe& candidate) const
    {
        if (!candidate.has_visuals())
        {
            throw std::invalid_argument(
                "MapMonitor needs a maps::MapRecipe with theme/classes/tiles visual layers."
            );
        }
        if (!renderer->has_map())
        {
            throw std::logic_error("Call AnimationRenderer::load_map(board) before creating MapMonitor.");
        }
        const RenderGrid& terrain = renderer->terrain_ids();
        if (candidate.rows() != static_cast<int>(terrain.size()) ||
            candidate.columns() != static_cast<int>(terrain.front().size()))
        {
            throw std::invalid_argument(
                "MapRecipe dimensions must match the Mapmaker already loaded by AnimationRenderer."
            );
        }
    }

    void rebuild_canvas()
    {
        require_compatible(recipe);
        canvas = std::make_unique<TileCanvas>(recipe.rows(), recipe.columns());
        canvas->draw(recipe.theme_id, recipe.classes, recipe.tiles, options.library_root);
        image = canvas_image(*canvas);
        if (image == nil)
        {
            throw std::runtime_error("Could not convert the tile canvas to a native monitor image.");
        }
    }

    void load_unit_sheets()
    {
        player_palette = load_gba_palette(fe8_asset_path(
            options.library_root, "graphics/unit_icon/palette/unit_icon_pal_player.agbpal"
        ));
        enemy_palette = load_gba_palette(fe8_asset_path(
            options.library_root, "graphics/unit_icon/palette/unit_icon_pal_enemy.agbpal"
        ));
        npc_palette = load_gba_palette(fe8_asset_path(
            options.library_root, "graphics/unit_icon/palette/unit_icon_pal_npc.agbpal"
        ));
        fourth_palette = load_gba_palette(fe8_asset_path(
            options.library_root, "graphics/unit_icon/palette/unit_icon_pal_p4.agbpal"
        ));
        raw_sheets = [[NSMutableDictionary alloc] init];
        sheets = [[NSMutableDictionary alloc] init];
        raw_wait_sheets = [[NSMutableDictionary alloc] init];
        wait_sheets = [[NSMutableDictionary alloc] init];
        white_sheets = [[NSMutableDictionary alloc] init];
        greyscale_sheets = [[NSMutableDictionary alloc] init];
        greyscale_wait_sheets = [[NSMutableDictionary alloc] init];
        item_icons = [[NSMutableDictionary alloc] init];
        opaque_tops = [[NSMutableDictionary alloc] init];
        miss_image = load_image(fe8_asset_path(
            options.library_root, "graphics/mapanim/miss.png"
        ));
        if (miss_image == nil)
        {
            throw std::runtime_error(
                "MapMonitor could not load the FE8 MISS map-animation glyph."
            );
        }

        // Bundled from FEBuilderGBA's FE8 text-engine source. It gives the
        // forecast real FE-style bitmap lettering while remaining completely
        // self-contained inside map_tile_library at runtime.
        NSImage* forecast_font = load_image(fe8_asset_path(
            options.library_root, "graphics/font/fe8_text_bold.png"
        ));
        if (forecast_font == nil)
        {
            throw std::runtime_error(
                "MapMonitor could not load the FE8 battle-forecast font atlas."
            );
        }
        forecast_font_ink = tint_fe8_font_atlas(
            forecast_font, [NSColor colorWithCalibratedWhite:0.06 alpha:1.0],
            &forecast_glyph_widths
        );
        forecast_font_white = tint_fe8_font_atlas(
            forecast_font, [NSColor colorWithCalibratedWhite:0.98 alpha:1.0]
        );
    }

    Palette palette_for(const GuildColor& color) const
    {
        switch (color.scheme)
        {
            case GuildColor::Scheme::Player: return player_palette;
            case GuildColor::Scheme::Enemy: return enemy_palette;
            case GuildColor::Scheme::Npc: return npc_palette;
            case GuildColor::Scheme::Fourth: return fourth_palette;
            case GuildColor::Scheme::Custom: return custom_team_palette(player_palette, color.rgb);
        }
        return player_palette;
    }

    NSImage* sheet_for(UnitVisual visual, const GuildColor& color)
    {
        const UnitVisualInfo& info = unit_visual_info(visual);
        const std::string visual_name(info.key);
        NSString* visual_key = [NSString stringWithUTF8String:visual_name.c_str()];
        NSString* key = [NSString stringWithFormat:@"%u:%@", palette_key(color), visual_key];
        NSImage* existing = sheets[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* raw = raw_sheets[visual_key];
        if (raw == nil)
        {
            raw = load_image(fe8_asset_path(options.library_root, info.move_png));
            if (raw == nil)
            {
                throw std::runtime_error(
                    "MapMonitor could not load FE8 map-unit sheet: " + std::string(info.move_png)
                );
            }
            raw_sheets[visual_key] = raw;
        }
        NSImage* coloured = restore_sprite_transparency(
            raw, player_palette, palette_for(color)
        );
        if (coloured != nil)
        {
            sheets[key] = coloured;
        }
        return coloured;
    }

    NSImage* white_sheet_for(UnitVisual visual, const GuildColor& color)
    {
        const UnitVisualInfo& info = unit_visual_info(visual);
        const std::string visual_name(info.key);
        NSString* visual_key = [NSString stringWithUTF8String:visual_name.c_str()];
        NSString* key = [NSString stringWithFormat:@"%u:%@", palette_key(color), visual_key];
        NSImage* existing = white_sheets[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* white = white_sprite(sheet_for(visual, color));
        if (white != nil)
        {
            white_sheets[key] = white;
        }
        return white;
    }

    NSImage* greyscale_sheet_for(UnitVisual visual, const GuildColor& color)
    {
        const UnitVisualInfo& info = unit_visual_info(visual);
        const std::string visual_name(info.key);
        NSString* visual_key = [NSString stringWithUTF8String:visual_name.c_str()];
        NSString* key = [NSString stringWithFormat:@"%u:%@", palette_key(color), visual_key];
        NSImage* existing = greyscale_sheets[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* greyscale = greyscale_sprite(sheet_for(visual, color));
        if (greyscale != nil)
        {
            greyscale_sheets[key] = greyscale;
        }
        return greyscale;
    }

    NSImage* wait_sheet_for(UnitVisual visual, const GuildColor& color)
    {
        const UnitVisualInfo& info = unit_visual_info(visual);
        if (info.wait_png.empty())
        {
            return nil;
        }
        const std::string visual_name(info.key);
        NSString* visual_key = [NSString stringWithUTF8String:visual_name.c_str()];
        NSString* key = [NSString stringWithFormat:@"%u:%@", palette_key(color), visual_key];
        NSImage* existing = wait_sheets[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* raw = raw_wait_sheets[visual_key];
        if (raw == nil)
        {
            raw = load_image(fe8_asset_path(options.library_root, info.wait_png));
            if (raw == nil)
            {
                throw std::runtime_error(
                    "MapMonitor could not load FE8 standing map-unit sheet: " +
                    std::string(info.wait_png)
                );
            }
            raw_wait_sheets[visual_key] = raw;
        }
        NSImage* coloured = restore_sprite_transparency(
            raw, player_palette, palette_for(color)
        );
        if (coloured != nil)
        {
            wait_sheets[key] = coloured;
        }
        return coloured;
    }

    NSImage* greyscale_wait_sheet_for(UnitVisual visual, const GuildColor& color)
    {
        const UnitVisualInfo& info = unit_visual_info(visual);
        if (info.wait_png.empty())
        {
            return nil;
        }
        const std::string visual_name(info.key);
        NSString* visual_key = [NSString stringWithUTF8String:visual_name.c_str()];
        NSString* key = [NSString stringWithFormat:@"%u:%@", palette_key(color), visual_key];
        NSImage* existing = greyscale_wait_sheets[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* greyscale = greyscale_sprite(wait_sheet_for(visual, color));
        if (greyscale != nil)
        {
            greyscale_wait_sheets[key] = greyscale;
        }
        return greyscale;
    }

    NSImage* item_icon_for(ItemID item)
    {
        const std::string_view relative = item_icon_for_forecast(item);
        if (relative.empty())
        {
            return nil;
        }
        NSString* key = [NSString stringWithUTF8String:std::string(relative).c_str()];
        NSImage* existing = item_icons[key];
        if (existing != nil)
        {
            return existing;
        }
        NSImage* icon = load_image(fe8_asset_path(options.library_root, relative));
        if (icon != nil)
        {
            item_icons[key] = icon;
        }
        return icon;
    }

    // The FE8 32x32 map-unit cells have transparent headroom. Scan each
    // visual/frame once and cache the first painted source row, so UI can
    // attach to actual artwork rather than to the oversized sprite canvas.
    CGFloat opaque_top(UnitVisual visual, const GuildColor& color, int sheet_cell)
    {
        NSString* key = [NSString stringWithFormat:@"%u:%u:%d",
                         palette_key(color),
                         static_cast<unsigned>(visual),
                         sheet_cell];
        NSNumber* cached = opaque_tops[key];
        if (cached != nil)
        {
            return cached.doubleValue;
        }

        NSImage* sheet = sheet_for(visual, color);
        NSRect proposed = NSMakeRect(0.0, 0.0, sheet.size.width, sheet.size.height);
        CGImageRef image = [sheet CGImageForProposedRect:&proposed context:nil hints:nil];
        if (image == nullptr)
        {
            return 0.0;
        }

        const std::size_t width = CGImageGetWidth(image);
        const std::size_t height = CGImageGetHeight(image);
        std::vector<std::uint8_t> pixels(width * height * 4, 0);
        CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            pixels.data(), width, height, 8, width * 4, colors,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
        );
        CGColorSpaceRelease(colors);
        if (context == nullptr)
        {
            return 0.0;
        }
        CGContextSetBlendMode(context, kCGBlendModeCopy);
        CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);
        CGContextRelease(context);

        const int first_row = std::max(0, sheet_cell) * 32;
        int top = 0;
        bool found = false;
        for (int y = 0; y < 32 && first_row + y < static_cast<int>(height); ++y)
        {
            for (int x = 0; x < 32 && x < static_cast<int>(width); ++x)
            {
                const std::size_t alpha = (static_cast<std::size_t>(first_row + y) * width + x) * 4 + 3;
                if (pixels[alpha] != 0)
                {
                    top = y;
                    found = true;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }

        opaque_tops[key] = @(top);
        return static_cast<CGFloat>(top);
    }

    CGFloat wait_opaque_top(UnitVisual visual, const GuildColor& color, int frame)
    {
        NSString* key = [NSString stringWithFormat:@"wait:%u:%u:%d",
                         palette_key(color),
                         static_cast<unsigned>(visual),
                         frame];
        NSNumber* cached = opaque_tops[key];
        if (cached != nil)
        {
            return cached.doubleValue;
        }

        NSImage* sheet = wait_sheet_for(visual, color);
        if (sheet == nil)
        {
            return 0.0;
        }
        NSRect proposed = NSMakeRect(0.0, 0.0, sheet.size.width, sheet.size.height);
        CGImageRef image = [sheet CGImageForProposedRect:&proposed context:nil hints:nil];
        if (image == nullptr)
        {
            return 0.0;
        }

        const std::size_t width = CGImageGetWidth(image);
        const std::size_t height = CGImageGetHeight(image);
        const int frame_height = std::max(1, static_cast<int>(height / 3));
        std::vector<std::uint8_t> pixels(width * height * 4, 0);
        CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            pixels.data(), width, height, 8, width * 4, colors,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
        );
        CGColorSpaceRelease(colors);
        if (context == nullptr)
        {
            return 0.0;
        }
        CGContextSetBlendMode(context, kCGBlendModeCopy);
        CGContextDrawImage(context, CGRectMake(0.0, 0.0, width, height), image);
        CGContextRelease(context);

        const int first_row = std::clamp(frame, 0, 2) * frame_height;
        int top = 0;
        bool found = false;
        for (int y = 0; y < frame_height && first_row + y < static_cast<int>(height); ++y)
        {
            for (int x = 0; x < static_cast<int>(width); ++x)
            {
                const std::size_t alpha =
                    (static_cast<std::size_t>(first_row + y) * width + x) * 4 + 3;
                if (pixels[alpha] != 0)
                {
                    top = y;
                    found = true;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }

        opaque_tops[key] = @(top);
        return static_cast<CGFloat>(top);
    }
};
}

static NSColor* route_arrow_color(std::size_t route_index)
{
    switch (route_index % 4)
    {
        case 0: return [NSColor colorWithCalibratedRed:1.0 green:0.89 blue:0.24 alpha:0.98];
        case 1: return [NSColor colorWithCalibratedRed:0.20 green:0.90 blue:1.0 alpha:0.98];
        case 2: return [NSColor colorWithCalibratedRed:1.0 green:0.36 blue:0.74 alpha:0.98];
        default: return [NSColor colorWithCalibratedRed:0.54 green:1.0 blue:0.32 alpha:0.98];
    }
}

static void draw_route_arrow(NSRect cell, int dx, int dy, NSColor* color)
{
    if (std::abs(dx) + std::abs(dy) != 1)
    {
        return;
    }

    const CGFloat pixel = std::max<CGFloat>(1.0, std::floor(NSWidth(cell) / 16.0));
    const CGFloat cx = NSMidX(cell);
    const CGFloat cy = NSMidY(cell);
    const CGFloat vx = static_cast<CGFloat>(dx);
    const CGFloat vy = static_cast<CGFloat>(dy);
    const CGFloat px = -vy;
    const CGFloat py = vx;

    [NSGraphicsContext saveGraphicsState];
    CGContextSetShouldAntialias([[NSGraphicsContext currentContext] CGContext], false);

    NSBezierPath* shaft = [NSBezierPath bezierPath];
    shaft.lineCapStyle = NSLineCapStyleButt;
    shaft.lineWidth = 4.0 * pixel;
    [shaft moveToPoint:NSMakePoint(cx - vx * 5.0 * pixel, cy - vy * 5.0 * pixel)];
    [shaft lineToPoint:NSMakePoint(cx + vx * 3.0 * pixel, cy + vy * 3.0 * pixel)];
    [[NSColor colorWithCalibratedWhite:0.04 alpha:0.90] setStroke];
    [shaft stroke];

    NSBezierPath* head = [NSBezierPath bezierPath];
    [head moveToPoint:NSMakePoint(cx + vx * 7.0 * pixel, cy + vy * 7.0 * pixel)];
    [head lineToPoint:NSMakePoint(cx + vx * 1.0 * pixel + px * 5.0 * pixel,
                                  cy + vy * 1.0 * pixel + py * 5.0 * pixel)];
    [head lineToPoint:NSMakePoint(cx + vx * 1.0 * pixel - px * 5.0 * pixel,
                                  cy + vy * 1.0 * pixel - py * 5.0 * pixel)];
    [head closePath];
    [[NSColor colorWithCalibratedWhite:0.04 alpha:0.90] setFill];
    [head fill];

    shaft.lineWidth = 2.0 * pixel;
    [color setStroke];
    [shaft stroke];

    NSBezierPath* inner_head = [NSBezierPath bezierPath];
    [inner_head moveToPoint:NSMakePoint(cx + vx * 6.0 * pixel, cy + vy * 6.0 * pixel)];
    [inner_head lineToPoint:NSMakePoint(cx + vx * 1.0 * pixel + px * 3.0 * pixel,
                                        cy + vy * 1.0 * pixel + py * 3.0 * pixel)];
    [inner_head lineToPoint:NSMakePoint(cx + vx * 1.0 * pixel - px * 3.0 * pixel,
                                        cy + vy * 1.0 * pixel - py * 3.0 * pixel)];
    [inner_head closePath];
    [color setFill];
    [inner_head fill];

    [NSGraphicsContext restoreGraphicsState];
}

static void draw_route_destination(NSRect cell, NSColor* color)
{
    const CGFloat pixel = std::max<CGFloat>(1.0, std::floor(NSWidth(cell) / 16.0));
    const CGFloat side = 6.0 * pixel;
    const NSRect box = NSMakeRect(NSMidX(cell) - side, NSMidY(cell) - side,
                                  side * 2.0, side * 2.0);
    [[NSColor colorWithCalibratedWhite:0.04 alpha:0.90] setFill];
    NSRectFill(box);
    [color setFill];
    NSRectFill(NSInsetRect(box, 2.0 * pixel, 2.0 * pixel));
}

@interface FE8MapMonitorView : NSView
{
    void* _state;
    NSRect _board;
    CGFloat _cell_pixels;
}
- (instancetype)initWithFrame:(NSRect)frame state:(void*)state;
@end

@implementation FE8MapMonitorView
- (instancetype)initWithFrame:(NSRect)frame state:(void*)state
{
    self = [super initWithFrame:frame];
    if (self)
    {
        _state = state;
    }
    return self;
}

- (BOOL)isFlipped { return YES; }

// A plain NSView does not accept keyboard focus unless it opts in. The monitor
// deliberately keeps all input in the normal C++ callback registered through
// MapMonitor::on_key().
- (BOOL)acceptsFirstResponder { return YES; }

- (BOOL)becomeFirstResponder { return YES; }

- (void)keyDown:(NSEvent*)event
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length == 0 || !state->key_callback)
    {
        return;
    }

    const unichar key = [characters characterAtIndex:0];
    if (key <= 0x7F)
    {
        state->key_callback(static_cast<char>(key));
        [self setNeedsDisplay:YES];
    }
}

- (void)layoutBoard
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    const CGFloat available_width = std::max<CGFloat>(
        1.0, self.bounds.size.width - kSidebarWidth - 3.0 * kMargin
    );
    const CGFloat available_height = std::max<CGFloat>(1.0, self.bounds.size.height - 2.0 * kMargin);
    const CGFloat fitted_cell = std::floor(std::min(
        available_width / state->recipe.columns(), available_height / state->recipe.rows()
    ));
    // FE8 terrain is 16x16 and its MU cells are 32x32. Snapping the display
    // cell to a 16-pixel multiple means terrain, 32px unit sprites, hit
    // flashes, and one-pixel lunges all receive an integer scale.
    _cell_pixels = fitted_cell >= 16.0
        ? std::floor(fitted_cell / 16.0) * 16.0
        : std::max<CGFloat>(1.0, fitted_cell);
    const CGFloat board_width = _cell_pixels * state->recipe.columns();
    const CGFloat board_height = _cell_pixels * state->recipe.rows();
    _board = NSMakeRect(kMargin, kMargin + (available_height - board_height) * 0.5,
                        board_width, board_height);
}

- (void)drawUnit:(const fe_tiles::UnitPose&)pose white:(BOOL)white
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    const bool greyscale = !white && state->renderer->turn_greyscale_enabled(pose.entity_id);
    NSImage* sheet = white ? state->white_sheet_for(pose.visual, pose.color)
                   : greyscale ? state->greyscale_sheet_for(pose.visual, pose.color)
                               : state->sheet_for(pose.visual, pose.color);
    if (sheet == nil)
    {
        return;
    }

    const CGFloat canvas_size = _cell_pixels * state->options.unit_canvas_in_tiles;
    const NSRect destination = NSMakeRect(
        NSMinX(_board) + (pose.x + 0.5) * _cell_pixels - canvas_size * 0.5,
        NSMinY(_board) + (pose.y + 1.0) * _cell_pixels - canvas_size,
        canvas_size, canvas_size
    );
    const NSRect source = NSMakeRect(
        0.0, sheet.size.height - (pose.sheet_cell + 1) * 32.0, 32.0, 32.0
    );

    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    [NSGraphicsContext saveGraphicsState];
    if (pose.flip_x)
    {
        NSAffineTransform* flip = [NSAffineTransform transform];
        [flip translateXBy:2.0 * NSMidX(destination) yBy:0.0];
        [flip scaleXBy:-1.0 yBy:1.0];
        [flip concat];
    }
    [sheet drawInRect:destination
              fromRect:source
             operation:NSCompositingOperationSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawWaitingUnit:(const fe_tiles::UnitPose&)pose
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    const bool greyscale = state->renderer->turn_greyscale_enabled(pose.entity_id);
    NSImage* sheet = greyscale
        ? state->greyscale_wait_sheet_for(pose.visual, pose.color)
        : state->wait_sheet_for(pose.visual, pose.color);
    if (sheet == nil)
    {
        [self drawUnit:pose white:NO];
        return;
    }

    // FE8's ordinary map sprites are separate SMS art. Each wait PNG packs
    // three equal-height frames, displayed globally as 0 for 32 ticks,
    // 1 for 4, 2 for 32, then 1 for 4: the original 72-tick standing loop.
    const int game_tick = static_cast<int>(state->idle_tick % 72);
    const int frame = game_tick < 32 ? 0 : game_tick < 36 ? 1 :
                      game_tick < 68 ? 2 : 1;
    const CGFloat source_width = sheet.size.width;
    const CGFloat source_height = sheet.size.height / 3.0;
    const CGFloat destination_width = _cell_pixels * source_width / 16.0;
    const CGFloat destination_height = _cell_pixels * source_height / 16.0;
    const NSRect destination = NSMakeRect(
        NSMinX(_board) + (pose.x + 0.5) * _cell_pixels - destination_width * 0.5,
        NSMinY(_board) + (pose.y + 1.0) * _cell_pixels - destination_height,
        destination_width, destination_height
    );
    const NSRect source = NSMakeRect(
        0.0, sheet.size.height - (frame + 1) * source_height,
        source_width, source_height
    );

    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    [sheet drawInRect:destination
              fromRect:source
             operation:NSCompositingOperationSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];
}

- (void)drawHealthBar:(const fe_tiles::HealthBar&)health
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);

    // Use the same 16-pixel scale as FE8's terrain cell so every edge and
    // every HP segment lands on a crisp pixel block.
    const CGFloat pixel = std::max<CGFloat>(1.0, std::floor(_cell_pixels / 16.0));
    constexpr int segments = 16;
    const CGFloat inside_width = segments * pixel;
    const CGFloat inside_height = 2.0 * pixel;
    // Two one-pixel caps make a stepped pixel-art ellipse/capsule rather
    // than a plain rectangle.
    const CGFloat outline_width = inside_width + 4.0 * pixel;
    const CGFloat outline_height = inside_height + 2.0 * pixel;

    // Follow the exact sprite representation currently being drawn. Away
    // from the cursor this is the small FE8 wait sheet; hovered, moving, and
    // combat units use the full 32x32 MU sheet.
    const bool stationary = std::floor(health.x) == health.x &&
                            std::floor(health.y) == health.y;
    const bool hovered = state->cursor.has_value() &&
        state->cursor->first == static_cast<int>(health.x) &&
        state->cursor->second == static_cast<int>(health.y);

    CGFloat art_top = 0.0;
    if (stationary && !hovered && !state->renderer->is_busy())
    {
        const int game_tick = static_cast<int>(state->idle_tick % 72);
        const int frame = game_tick < 32 ? 0 : game_tick < 36 ? 1 :
                          game_tick < 68 ? 2 : 1;
        NSImage* sheet = state->wait_sheet_for(health.visual, health.color);
        if (sheet != nil)
        {
            const CGFloat source_height = sheet.size.height / 3.0;
            const CGFloat destination_height = _cell_pixels * source_height / 16.0;
            const CGFloat sprite_top = NSMinY(_board) +
                (health.y + 1.0) * _cell_pixels - destination_height;
            art_top = sprite_top + state->wait_opaque_top(
                health.visual, health.color, frame
            ) * destination_height / source_height;
        }
    }

    if (art_top == 0.0)
    {
        const CGFloat canvas_size = _cell_pixels * state->options.unit_canvas_in_tiles;
        const CGFloat sprite_top = NSMinY(_board) +
            (health.y + 1.0) * _cell_pixels - canvas_size;
        art_top = sprite_top + state->opaque_top(
            health.visual, health.color, health.sheet_cell
        ) * canvas_size / 32.0;
    }
    const CGFloat x = NSMinX(_board) + (health.x + 0.5) * _cell_pixels - outline_width * 0.5;
    const CGFloat y = art_top - outline_height - pixel;
    const NSRect outline = NSMakeRect(x, y, outline_width, outline_height);
    const NSRect background = NSMakeRect(x + 2.0 * pixel, y + pixel,
                                         inside_width, inside_height);

    const double ratio = std::clamp(
        health.displayed_hp / static_cast<double>(std::max(1, health.maximum_hp)),
        0.0, 1.0
    );
    NSColor* fill = ratio > 0.50
        ? [NSColor colorWithCalibratedRed:0.18 green:0.88 blue:0.29 alpha:0.98]
        : ratio > 0.25
            ? [NSColor colorWithCalibratedRed:0.97 green:0.75 blue:0.18 alpha:0.98]
            : [NSColor colorWithCalibratedRed:0.92 green:0.22 blue:0.18 alpha:0.98];

    [NSGraphicsContext saveGraphicsState];
    CGContextSetShouldAntialias([[NSGraphicsContext currentContext] CGContext], false);
    [[NSColor colorWithCalibratedWhite:0.02 alpha:0.96] setFill];
    // A pair of crossed rectangles removes one pixel from each corner. It
    // reads as a rounded FE/GBA capsule while remaining literally pixelated.
    NSRectFill(NSMakeRect(NSMinX(outline) + pixel, NSMinY(outline),
                          NSWidth(outline) - 2.0 * pixel, NSHeight(outline)));
    NSRectFill(NSMakeRect(NSMinX(outline), NSMinY(outline) + pixel,
                          NSWidth(outline), NSHeight(outline) - 2.0 * pixel));
    [[NSColor colorWithCalibratedWhite:0.12 alpha:1.0] setFill];
    NSRectFill(background);

    const int filled_segments = static_cast<int>(std::round(ratio * segments));
    if (filled_segments > 0)
    {
        const NSRect fill_rect = NSMakeRect(
            NSMinX(background), NSMinY(background), filled_segments * pixel, inside_height
        );
        [fill setFill];
        NSRectFill(fill_rect);
    }
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawRect:(NSRect)dirtyRect
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    [[NSColor colorWithCalibratedRed:0.035 green:0.05 blue:0.09 alpha:1.0] setFill];
    NSRectFill(self.bounds);
    [self layoutBoard];

    [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
    [state->image drawInRect:_board
                    fromRect:NSMakeRect(0, 0, state->image.size.width, state->image.size.height)
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0
              respectFlipped:YES
                       hints:nil];

    const fe_tiles::RenderGrid& blue = state->renderer->blue_tiles();
    const fe_tiles::RenderGrid& red = state->renderer->red_tiles();
    for (int y = 0; y < state->recipe.rows(); ++y)
    {
        for (int x = 0; x < state->recipe.columns(); ++x)
        {
            const bool movement = blue[y][x] >= 0;
            const bool attack_only = !movement && red[y][x] != 0;
            if (!movement && !attack_only)
            {
                continue;
            }
            const NSRect cell = NSMakeRect(
                NSMinX(_board) + x * _cell_pixels,
                NSMinY(_board) + y * _cell_pixels,
                _cell_pixels, _cell_pixels
            );
            if (movement)
            {
                [[NSColor colorWithCalibratedRed:0.08 green:0.42 blue:1.0 alpha:0.34] setFill];
            }
            else
            {
                [[NSColor colorWithCalibratedRed:0.72 green:0.17 blue:0.10 alpha:0.46] setFill];
            }
            [[NSBezierPath bezierPathWithRect:NSInsetRect(cell, 1.0, 1.0)] fill];
        }
    }

    // Route arrows are independent of blue/red action paint. They are useful
    // for visualising any already-computed generalized path without letting
    // the monitor infer, commit, or otherwise own that path.
    for (std::size_t route_index = 0; route_index < state->route_arrows.size(); ++route_index)
    {
        const std::vector<std::pair<int, int>>& route = state->route_arrows[route_index];
        NSColor* color = route_arrow_color(route_index);
        for (std::size_t index = 0; index < route.size(); ++index)
        {
            const auto [x, y] = route[index];
            const NSRect cell = NSMakeRect(
                NSMinX(_board) + x * _cell_pixels,
                NSMinY(_board) + y * _cell_pixels,
                _cell_pixels, _cell_pixels
            );

            if (index + 1 == route.size())
            {
                draw_route_destination(cell, color);
                continue;
            }

            const auto [next_x, next_y] = route[index + 1];
            draw_route_arrow(cell, next_x - x, next_y - y, color);
        }
    }

    for (const fe_tiles::UnitPose& pose : state->renderer->unit_poses())
    {
        const bool stationary = std::floor(pose.x) == pose.x && std::floor(pose.y) == pose.y;
        const bool hovered = state->cursor.has_value() &&
            state->cursor->first == static_cast<int>(pose.x) &&
            state->cursor->second == static_cast<int>(pose.y);

        // Away from the cursor FE8 uses the small, three-frame SMS wait
        // artwork. Cursor-over-unit and every active presentation retain the
        // existing full 32x32 selected/moving MU animation.
        if (stationary && !hovered && !state->renderer->is_busy() &&
            pose.sheet_cell >= 12)
        {
            [self drawWaitingUnit:pose];
        }
        else
        {
            [self drawUnit:pose white:NO];
        }
    }
    for (const fe_tiles::HealthBar& health : state->renderer->health_bars())
    {
        [self drawHealthBar:health];
    }

    // Two-state pixel cursor. The wide state places its corner vertices just
    // outside the map cell; the tight state places them just inside it. The
    // four L corners therefore breathe around every cell indefinitely,
    // regardless of terrain or unit occupancy.
    if (state->cursor)
    {
        const int x = state->cursor->first;
        const int y = state->cursor->second;
        const bool wide = static_cast<int>(std::floor(
            [[NSDate date] timeIntervalSinceReferenceDate] * 5.0
        )) % 2 == 0;
        // Four independent 4x4 pixel L glyphs. Keeping the arms below half
        // a tile is essential: 8px arms meet in the middle and become a
        // plain rectangle instead of four visible cursor corners.
        const CGFloat corner = _cell_pixels * 4.0 / 16.0;
        const CGFloat stroke = std::max<CGFloat>(1.0, _cell_pixels / 16.0);
        // Frame 1: the inward ends of a 4px corner glyph meet the outside
        // of the cell corner. Frame 2: the glyph's outer vertex is exactly
        // on the inside of that same cell corner. This is a distinct 4px
        // jump between four detached corner glyphs.
        const CGFloat edge_offset = wide ? corner : 0.0;
        const CGFloat left = NSMinX(_board) + x * _cell_pixels - edge_offset;
        const CGFloat top = NSMinY(_board) + y * _cell_pixels - edge_offset;
        const CGFloat right = NSMinX(_board) + (x + 1) * _cell_pixels + edge_offset;
        const CGFloat bottom = NSMinY(_board) + (y + 1) * _cell_pixels + edge_offset;

        // Dark one-pixel shadow first, then FE's bright white cursor.
        for (const CGFloat shadow : {stroke, 0.0})
        {
            if (shadow > 0.0)
            {
                [[NSColor colorWithCalibratedWhite:0.12 alpha:0.90] setStroke];
            }
            else
            {
                [[NSColor colorWithCalibratedWhite:1.0 alpha:1.0] setStroke];
            }
            NSBezierPath* cursor = [NSBezierPath bezierPath];
            cursor.lineWidth = stroke;
            [cursor moveToPoint:NSMakePoint(left + shadow, top + corner + shadow)];
            [cursor lineToPoint:NSMakePoint(left + shadow, top + shadow)];
            [cursor lineToPoint:NSMakePoint(left + corner + shadow, top + shadow)];
            [cursor moveToPoint:NSMakePoint(right - corner + shadow, top + shadow)];
            [cursor lineToPoint:NSMakePoint(right + shadow, top + shadow)];
            [cursor lineToPoint:NSMakePoint(right + shadow, top + corner + shadow)];
            [cursor moveToPoint:NSMakePoint(left + shadow, bottom - corner + shadow)];
            [cursor lineToPoint:NSMakePoint(left + shadow, bottom + shadow)];
            [cursor lineToPoint:NSMakePoint(left + corner + shadow, bottom + shadow)];
            [cursor moveToPoint:NSMakePoint(right - corner + shadow, bottom + shadow)];
            [cursor lineToPoint:NSMakePoint(right + shadow, bottom + shadow)];
            [cursor lineToPoint:NSMakePoint(right + shadow, bottom - corner + shadow)];
            [cursor stroke];
        }
    }

    if (const auto& hit = state->renderer->hit_effect(); hit.has_value())
    {
        // FE8 uses a temporary OBJ palette, not a modern glow. A normal hit
        // starts white and fades over 17 ticks. A critical alternates the
        // flash palette and normal palette, then shakes by +/- two original
        // pixels before the fade back.
        CGFloat opacity = 0.0;
        if (hit->critical && hit->tick < 12)
        {
            const int tick = hit->tick;
            const bool flash = tick < 2 || (tick >= 5 && tick < 7) || tick >= 10;
            opacity = flash ? 1.0 : 0.0;
        }
        else if (hit->critical)
        {
            opacity = std::clamp(
                (29.0 - static_cast<CGFloat>(hit->tick)) / 17.0, 0.0, 1.0
            );
        }
        else
        {
            opacity = std::clamp(
                (17.0 - static_cast<CGFloat>(hit->tick)) / 17.0, 0.0, 0.90
            );
        }

        if (opacity > 0.0)
        {
            [NSGraphicsContext saveGraphicsState];
            if (hit->critical && hit->tick >= 12 && hit->tick < 24)
            {
                const CGFloat original_pixel = _cell_pixels / 16.0;
                const CGFloat shake = (hit->tick & 1) ? 2.0 : -2.0;
                NSAffineTransform* transform = [NSAffineTransform transform];
                [transform translateXBy:shake * original_pixel yBy:0.0];
                [transform concat];
            }
            CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], opacity);
            [self drawUnit:hit->pose white:YES];
            [NSGraphicsContext restoreGraphicsState];
        }
    }
    for (const fe_tiles::DeathEffect& effect : state->renderer->death_effects())
    {
        // DeathEffect exists for the lethal-impact frame only. The 17-frame
        // white glow is already drawn from HitEffect above, matching FE8's
        // StartMuHitFlash without a second pause after the hit.
        [NSGraphicsContext saveGraphicsState];
        [self drawUnit:effect.pose white:YES];
        [NSGraphicsContext restoreGraphicsState];
    }
    if (const auto& miss = state->renderer->miss_effect(); miss.has_value())
    {
        // FE8's Obj_MapAnimMISS is a 32x8 pixel-art OBJ, not a font label.
        // Its AP script starts very large then settles to normal scale during
        // its first nine 60 Hz frames. Keep the source pixels crisp while
        // using that same readable pop-in shape.
        const CGFloat settle = std::clamp(static_cast<CGFloat>(miss->tick) / 8.0,
                                          0.0, 1.0);
        const CGFloat scale = 1.65 - 0.65 * settle;
        const CGFloat width = 2.0 * _cell_pixels * scale;
        const CGFloat height = 0.5 * _cell_pixels * scale;
        const CGFloat fade = miss->tick <= 15
            ? 1.0
            : std::clamp((20.0 - static_cast<CGFloat>(miss->tick)) / 5.0, 0.0, 1.0);
        const NSRect popup = NSMakeRect(
            NSMinX(_board) + (miss->x + 0.5) * _cell_pixels - width * 0.5,
            NSMinY(_board) + (miss->y + 0.20) * _cell_pixels -
                (height - 0.5 * _cell_pixels) * 0.5,
            width, height
        );

        [NSGraphicsContext saveGraphicsState];
        CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], fade);
        [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
        [state->miss_image drawInRect:popup
                              fromRect:NSMakeRect(0.0, 0.0,
                                                  state->miss_image.size.width,
                                                  state->miss_image.size.height)
                             operation:NSCompositingOperationSourceOver
                              fraction:1.0
                        respectFlipped:YES
                                 hints:nil];
        [NSGraphicsContext restoreGraphicsState];
    }

    if (state->phase_intro.has_value())
    {
        // FE8's phase intro is driven by three parallel proc scripts:
        // squares: 34 frames in + 36 frames out; title: wait 6, 16 in,
        // hold 30, 16 out; blend-box: 32 in + 33 out. `tick` below keeps
        // the title's 68-frame lifetime, which is the last script to finish.
        const fe_tiles::MapMonitor::Impl::PhaseIntro& phase = *state->phase_intro;
        const int tick = phase.tick;
        const CGFloat board_width = NSWidth(_board);
        const CGFloat board_height = NSHeight(_board);
        const std::uint32_t rgb = phase.color.rgb;
        const CGFloat red = static_cast<CGFloat>((rgb >> 16) & 0xFF) / 255.0;
        const CGFloat green = static_cast<CGFloat>((rgb >> 8) & 0xFF) / 255.0;
        const CGFloat blue_component = static_cast<CGFloat>(rgb & 0xFF) / 255.0;
        const auto clamp01 = [](CGFloat value)
        {
            return std::clamp(value, 0.0, 1.0);
        };
        const auto ease_out_cubic = [](CGFloat value)
        {
            const CGFloat inverse = 1.0 - std::clamp(value, 0.0, 1.0);
            return 1.0 - inverse * inverse * inverse;
        };
        const auto ease_in_cubic = [](CGFloat value)
        {
            value = std::clamp(value, 0.0, 1.0);
            return value * value * value;
        };

        [NSGraphicsContext saveGraphicsState];
        CGContextRef phase_context = [[NSGraphicsContext currentContext] CGContext];
        CGContextClipToRect(phase_context, NSRectToCGRect(_board));
        CGContextSetShouldAntialias(phase_context, false);

        // `PhaseIntroSquares_InLoop` operates on a 15x10 GBA-screen grid.
        // Keep that exact virtual grid regardless of the real map dimensions,
        // so an oversized Chapter 18 board does not make the sweep sluggish.
        constexpr int screen_columns = 15;
        constexpr int screen_rows = 10;
        const CGFloat square_width = board_width / screen_columns;
        const CGFloat square_height = board_height / screen_rows;
        const bool squares_exiting = tick >= 34;
        const int square_tick = squares_exiting ? tick - 34 : tick + 4;
        for (int row = 0; row < screen_rows; ++row)
        {
            for (int column = 0; column < screen_columns; ++column)
            {
                // The two formulas follow PhaseIntroSquares_InLoop and
                // PhaseIntroSquares_OutLoop respectively. The source changes
                // one 16px tile graphic per diagonal step; alpha is the
                // desktop equivalent of those 0..16 source tile variants.
                int source_value = 0;
                if (!squares_exiting)
                {
                    source_value = (column - square_tick) + (0x15 - row);
                    source_value = std::clamp(source_value, 0, 0x10);
                    source_value = (0x10 - source_value) & 0xFE;
                }
                else
                {
                    source_value = (1 - square_tick) + (10 + column) + (10 - row);
                    source_value = std::clamp(source_value, 0, 0x10) & 0xFE;
                    source_value = 0x10 - source_value;
                }
                const CGFloat amount = clamp01(static_cast<CGFloat>(source_value) / 16.0);
                if (amount <= 0.0)
                {
                    continue;
                }
                [[NSColor colorWithCalibratedRed:red * 0.42
                                            green:green * 0.42
                                             blue:blue_component * 0.42
                                            alpha:0.72 * amount] setFill];
                NSRectFill(NSMakeRect(NSMinX(_board) + column * square_width,
                                      NSMinY(_board) + row * square_height,
                                      std::ceil(square_width) + 1.0,
                                      std::ceil(square_height) + 1.0));
            }
        }

        // PhaseIntroBlendBox expands a centred 96px-tall source window,
        // blending the map behind it. Its timer begins at four and reaches
        // the full box at source frame 32, then contracts over 33 frames.
        CGFloat box_progress = 1.0;
        if (tick < 28)
        {
            box_progress = ease_out_cubic(static_cast<CGFloat>(tick + 4) / 32.0);
        }
        else if (tick < 61)
        {
            box_progress = ease_in_cubic(static_cast<CGFloat>(61 - tick) / 33.0);
        }
        const CGFloat band_height = std::max<CGFloat>(
            _cell_pixels * 1.8, board_height * 0.30
        ) * box_progress;
        const CGFloat band_y = NSMidY(_board) - band_height * 0.5;
        const NSRect band = NSMakeRect(NSMinX(_board), band_y, board_width, band_height);
        [[NSColor colorWithCalibratedWhite:0.015 alpha:0.62 * box_progress] setFill];
        NSRectFill(band);
        [[NSColor colorWithCalibratedRed:red * 0.55
                                    green:green * 0.55
                                     blue:blue_component * 0.55
                                    alpha:0.50 * box_progress] setFill];
        NSRectFill(NSInsetRect(band, 0.0, std::max<CGFloat>(1.0, _cell_pixels / 16.0)));

        // `PhaseIntroText_InLoop`: wait six, RCUBIC -0x1C -> -8 for 16
        // frames. `OutLoop`: CUBIC -0x1C -> -0x38 for another 16. Its real
        // GBA title graphic is static, but the display string must be dynamic
        // for Stochesia guilds, hence the bundled literal FE8 bitmap font.
        if (tick >= 6 && tick < 68)
        {
            CGFloat title_progress = 1.0;
            CGFloat title_x = NSMidX(_board);
            if (tick < 22)
            {
                title_progress = ease_out_cubic(static_cast<CGFloat>(tick - 6) / 16.0);
                title_x = NSMinX(_board) - board_width * (1.0 - title_progress);
            }
            else if (tick >= 52)
            {
                title_progress = 1.0 - ease_in_cubic(static_cast<CGFloat>(tick - 52) / 16.0);
                title_x = NSMidX(_board) - board_width * (1.0 - title_progress);
            }

            std::string label = phase.guild_name + "'S PHASE";
            for (char& character : label)
            {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            NSString* text = [NSString stringWithUTF8String:label.c_str()];
            const CGFloat title_scale = std::clamp(_cell_pixels / 16.0, 1.0, 2.0);
            CGFloat source_width = 0.0;
            for (NSUInteger index = 0; index < text.length; ++index)
            {
                const unichar character = [text characterAtIndex:index];
                source_width += state->forecast_glyph_widths[
                    character < state->forecast_glyph_widths.size() ? character : '?'
                ];
            }
            const CGFloat title_width = source_width * title_scale;
            const CGFloat title_height = 16.0 * title_scale;
            const NSRect title_box = NSMakeRect(
                title_x - title_width * 0.5 - 8.0 * title_scale,
                NSMidY(_board) - title_height * 0.5 - 5.0 * title_scale,
                title_width + 16.0 * title_scale,
                title_height + 10.0 * title_scale
            );
            [[NSColor colorWithCalibratedWhite:0.02 alpha:0.94] setFill];
            NSRectFill(title_box);
            [[NSColor colorWithCalibratedRed:red * 0.72 + 0.10
                                        green:green * 0.72 + 0.10
                                         blue:blue_component * 0.72 + 0.10
                                        alpha:0.95] setFill];
            NSRectFill(NSInsetRect(title_box, 2.0 * title_scale, 2.0 * title_scale));
            [[NSColor colorWithCalibratedWhite:0.04 alpha:0.90] setFill];
            NSRectFill(NSInsetRect(title_box, 4.0 * title_scale, 4.0 * title_scale));
            draw_fe8_outlined_text(
                state->forecast_font_white, state->forecast_font_ink,
                state->forecast_glyph_widths, text,
                NSMakePoint(title_x - title_width * 0.5,
                            NSMidY(_board) - title_height * 0.5),
                title_scale
            );
        }
        [NSGraphicsContext restoreGraphicsState];
    }

    [[NSColor colorWithWhite:1.0 alpha:0.2] setStroke];
    [[NSBezierPath bezierPathWithRect:_board] stroke];

    if (state->battle_forecast.has_value())
    {
        const fe_tiles::MapMonitor::Impl::BattleForecast& forecast = *state->battle_forecast;
        const CGFloat side_x = NSMaxX(_board) + kMargin;
        const CGFloat panel_width = kSidebarWidth - kMargin;
        // A persistent phase dialogue occupies the top of the sidebar. Put
        // the combat forecast immediately below it rather than letting the
        // two panels overlap. Constrain its scale by remaining height too.
        const CGFloat phase_dialogue_height = state->phase_dialogue.has_value()
            ? std::min<CGFloat>(158.0, self.bounds.size.height - 2.0 * kMargin)
            : 0.0;
        const CGFloat forecast_y = state->phase_dialogue.has_value()
            ? kMargin + 16.0 + phase_dialogue_height + 12.0
            : 26.0;
        const CGFloat remaining_height = std::max<CGFloat>(
            1.0, self.bounds.size.height - forecast_y - kMargin
        );
        // FE8's standard battle forecast is exactly 120x176 source pixels.
        // All geometry below deliberately remains expressed in that original
        // coordinate system, so the monitor is a scaled GBA panel rather
        // than a modern sidebar card that merely contains the same numbers.
        const CGFloat scale = std::min({
            1.80,
            (panel_width - 8.0) / 120.0,
            remaining_height / 176.0
        });
        const CGFloat panel_w = 120.0 * scale;
        const CGFloat panel_h = 176.0 * scale;
        const CGFloat px = side_x + (panel_width - panel_w) * 0.5;
        const CGFloat py = forecast_y;
        const auto rect = [=](CGFloat x, CGFloat y, CGFloat w, CGFloat h)
        {
            return NSMakeRect(px + x * scale, py + y * scale, w * scale, h * scale);
        };
        const auto fill = [&rect](NSColor* color, CGFloat x, CGFloat y, CGFloat w, CGFloat h)
        {
            [color setFill];
            NSRectFill(rect(x, y, w, h));
        };
        const auto draw_icon = [&](ItemID item, CGFloat x, CGFloat y)
        {
            NSImage* icon = state->item_icon_for(item);
            if (icon == nil)
            {
                return;
            }
            [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationNone];
            [icon drawInRect:rect(x, y, 16.0, 16.0)
                   fromRect:NSMakeRect(0.0, 0.0, icon.size.width, icon.size.height)
                  operation:NSCompositingOperationSourceOver
                   fraction:1.0
             respectFlipped:YES
                   hints:nil];
        };

        NSColor* outline = [NSColor colorWithCalibratedRed:0.055 green:0.043 blue:0.047 alpha:1.0];
        NSColor* gold_edge = [NSColor colorWithCalibratedRed:0.93 green:0.82 blue:0.51 alpha:1.0];
        NSColor* gold_highlight = [NSColor colorWithCalibratedRed:1.0 green:0.94 blue:0.72 alpha:1.0];
        NSColor* actor_blue = [NSColor colorWithCalibratedRed:0.22 green:0.39 blue:0.62 alpha:1.0];
        NSColor* actor_blue_light = [NSColor colorWithCalibratedRed:0.30 green:0.49 blue:0.72 alpha:1.0];
        NSColor* actor_blue_dark = [NSColor colorWithCalibratedRed:0.12 green:0.25 blue:0.43 alpha:1.0];
        NSColor* target_red = [NSColor colorWithCalibratedRed:0.70 green:0.29 blue:0.30 alpha:1.0];
        NSColor* target_red_light = [NSColor colorWithCalibratedRed:0.79 green:0.36 blue:0.35 alpha:1.0];
        NSColor* target_red_dark = [NSColor colorWithCalibratedRed:0.50 green:0.16 blue:0.18 alpha:1.0];
        NSColor* parchment = [NSColor colorWithCalibratedRed:0.92 green:0.86 blue:0.62 alpha:1.0];
        NSColor* parchment_light = [NSColor colorWithCalibratedRed:0.98 green:0.93 blue:0.73 alpha:1.0];
        NSColor* parchment_shadow = [NSColor colorWithCalibratedRed:0.73 green:0.65 blue:0.42 alpha:1.0];

        const auto texture = [&](NSColor* base, NSColor* fleck,
                                 int x, int y, int width, int height, int seed)
        {
            fill(base, x, y, width, height);
            for (int yy = y + 1; yy < y + height - 1; yy += 2)
            {
                for (int xx = x + 1; xx < x + width - 1; xx += 2)
                {
                    if (((xx * 11 + yy * 7 + seed) % 13) < 3)
                    {
                        fill(fleck, xx, yy, 1.0, 1.0);
                    }
                }
            }
        };

        // The original has a black/gold/black frame, then four textured
        // fields: actor blue, defender red, parchment labels, and red footer.
        fill(outline, 0.0, 0.0, 120.0, 176.0);
        fill(gold_highlight, 2.0, 2.0, 116.0, 172.0);
        fill(gold_edge, 3.0, 3.0, 114.0, 170.0);
        fill(outline, 5.0, 5.0, 110.0, 166.0);
        texture(actor_blue, actor_blue_light, 6, 6, 108, 26, 1);
        texture(target_red, target_red_light, 6, 32, 38, 98, 2);
        texture(parchment, parchment_light, 44, 32, 28, 98, 3);
        texture(actor_blue_dark, actor_blue, 72, 32, 42, 98, 4);
        texture(target_red_dark, target_red, 6, 130, 108, 40, 5);

        // A few hard palette edges make the faux tile fields read like FE8's
        // UI graphics rather than a flat CSS rectangle.
        fill(gold_edge, 43.0, 32.0, 1.0, 98.0);
        fill(parchment_shadow, 70.0, 32.0, 2.0, 98.0);
        fill(outline, 6.0, 31.0, 108.0, 1.0);
        fill(outline, 6.0, 129.0, 108.0, 1.0);

        const CGFloat name_scale = scale * 0.76;
        const CGFloat stat_scale = scale * 0.96;
        const CGFloat label_scale = scale * 0.63;
        const CGFloat item_scale = scale * 0.66;

        draw_icon(forecast.attacker_weapon, 8.0, 10.0);
        draw_icon(forecast.defender_weapon, 92.0, 143.0);
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths,
            [NSString stringWithUTF8String:forecast.attacker_name.c_str()],
            NSMakePoint(px + 29.0 * scale, py + 8.0 * scale), name_scale
        );
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths,
            [NSString stringWithUTF8String:forecast.defender_name.c_str()],
            NSMakePoint(px + 9.0 * scale, py + 135.0 * scale), name_scale
        );
        const std::string defender_item = item_name_for_forecast(forecast.defender_weapon);
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths,
            [NSString stringWithUTF8String:defender_item.c_str()],
            NSMakePoint(px + 9.0 * scale, py + 153.0 * scale), item_scale
        );

        const std::array<NSString*, 4> labels = {@"HP", @"Mt", @"Hit", @"Crit"};
        const std::array<int, 4> actor_values = {
            forecast.attacker_hp,
            forecast.attacker_combat.MT,
            forecast.attacker_combat.HIT,
            forecast.attacker_combat.CRIT
        };
        const std::array<int, 4> defender_values = {
            forecast.defender_hp,
            forecast.defender_combat.MT,
            forecast.defender_combat.HIT,
            forecast.defender_combat.CRIT
        };
        for (int row = 0; row < 4; ++row)
        {
            const CGFloat y = 38.0 + static_cast<CGFloat>(row) * 22.0;
            draw_fe8_bitmap_text(
                state->forecast_font_ink, state->forecast_glyph_widths,
                labels[row], NSMakePoint(px + 49.0 * scale, py + y * scale), label_scale
            );

            const std::string actor = forecast.attacker_weapon == NO_ITEM && row > 0
                ? "--" : std::to_string(actor_values[row]);
            const std::string defender = forecast.defender_weapon == NO_ITEM && row > 0
                ? "--" : std::to_string(defender_values[row]);
            draw_fe8_outlined_text(
                state->forecast_font_white, state->forecast_font_ink,
                state->forecast_glyph_widths,
                [NSString stringWithUTF8String:defender.c_str()],
                NSMakePoint(px + 8.0 * scale, py + (y - 3.0) * scale), stat_scale
            );
            draw_fe8_outlined_text(
                state->forecast_font_white, state->forecast_font_ink,
                state->forecast_glyph_widths,
                [NSString stringWithUTF8String:actor.c_str()],
                NSMakePoint(px + 78.0 * scale, py + (y - 3.0) * scale), stat_scale
            );
        }

        // FE8's `PutBattleForecastMultipliers()` advances 4 angle units per
        // frame, producing an ellipse of radius 4x2 source pixels around Mt.
        const double angle = static_cast<double>((state->idle_tick * 4) & 0xFF) *
                             (2.0 * M_PI / 256.0);
        const CGFloat orbit_x = static_cast<CGFloat>(std::sin(angle) * 4.0) * scale;
        const CGFloat orbit_y = static_cast<CGFloat>(std::cos(angle) * 2.0) * scale;
        const auto draw_multiplier = [&](bool doubled, CGFloat base_x)
        {
            if (!doubled)
            {
                return;
            }
            const NSRect badge = NSMakeRect(px + base_x * scale + orbit_x,
                                            py + 53.0 * scale + orbit_y,
                                            15.0 * scale, 11.0 * scale);
            [[NSColor colorWithCalibratedRed:0.09 green:0.06 blue:0.07 alpha:0.95] setFill];
            [[NSBezierPath bezierPathWithOvalInRect:badge] fill];
            [[NSColor colorWithCalibratedRed:0.96 green:0.80 blue:0.25 alpha:1.0] setStroke];
            [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(badge, scale * 0.5, scale * 0.5)] stroke];
            draw_fe8_outlined_text(
                state->forecast_font_white, state->forecast_font_ink,
                state->forecast_glyph_widths, @"x2",
                NSMakePoint(NSMinX(badge) + 2.0 * scale,
                            NSMinY(badge) - 1.0 * scale),
                scale * 0.42
            );
        };
        draw_multiplier(forecast.defender_combat.DB, 18.0);
        draw_multiplier(forecast.attacker_combat.DB, 78.0);
    }

    // The map receives FE8's animated phase card above. Mirror its title in
    // the monitor sidebar as well, so the active phase remains explicit even
    // on a large or visually busy custom map. While active, it deliberately
    // takes precedence over a stale battle forecast.
    if (state->phase_dialogue.has_value())
    {
        const fe_tiles::MapMonitor::Impl::PhaseDialogue& phase = *state->phase_dialogue;
        CGFloat title_alpha = 1.0;
        // Match the map title during its entry/exit, then keep this sidebar
        // panel fully visible after the transient map animation is gone.
        if (state->phase_intro.has_value())
        {
            const int tick = state->phase_intro->tick;
            if (tick < 6)
            {
                title_alpha = 0.0;
            }
            else if (tick < 22)
            {
                const CGFloat progress = static_cast<CGFloat>(tick - 6) / 16.0;
                const CGFloat inverse = 1.0 - std::clamp(progress, 0.0, 1.0);
                title_alpha = 1.0 - inverse * inverse * inverse;
            }
            else if (tick >= 52)
            {
                const CGFloat progress = std::clamp(
                    static_cast<CGFloat>(tick - 52) / 16.0, 0.0, 1.0
                );
                title_alpha = 1.0 - progress * progress * progress;
            }
        }

        if (title_alpha > 0.0)
        {
            std::string guild_label = phase.guild_name;
            for (char& character : guild_label)
            {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            NSString* guild_text = [NSString stringWithUTF8String:guild_label.c_str()];
            NSString* phase_text = @"PHASE";
            const CGFloat side_x = NSMaxX(_board) + kMargin;
            const CGFloat panel_width = std::max<CGFloat>(
                1.0, self.bounds.size.width - side_x - kMargin
            );
            const CGFloat panel_height = std::min<CGFloat>(158.0, self.bounds.size.height - 2.0 * kMargin);
            const NSRect panel = NSMakeRect(side_x, kMargin + 16.0, panel_width, panel_height);
            const std::uint32_t rgb = phase.color.rgb;
            const CGFloat red = static_cast<CGFloat>((rgb >> 16) & 0xFF) / 255.0;
            const CGFloat green = static_cast<CGFloat>((rgb >> 8) & 0xFF) / 255.0;
            const CGFloat blue_component = static_cast<CGFloat>(rgb & 0xFF) / 255.0;
            const auto text_source_width = [&](NSString* text)
            {
                CGFloat width = 0.0;
                for (NSUInteger index = 0; index < text.length; ++index)
                {
                    const unichar character = [text characterAtIndex:index];
                    width += state->forecast_glyph_widths[
                        character < state->forecast_glyph_widths.size() ? character : '?'
                    ];
                }
                return width;
            };
            const CGFloat guild_source_width = text_source_width(guild_text);
            const CGFloat guild_scale = std::clamp(
                (panel_width - 34.0) / std::max<CGFloat>(1.0, guild_source_width), 0.70, 1.35
            );
            const CGFloat phase_scale = std::min<CGFloat>(1.55, guild_scale + 0.20);
            const CGFloat guild_width = guild_source_width * guild_scale;
            const CGFloat phase_width = text_source_width(phase_text) * phase_scale;

            [NSGraphicsContext saveGraphicsState];
            CGContextSetAlpha([[NSGraphicsContext currentContext] CGContext], title_alpha);
            [[NSColor colorWithCalibratedWhite:0.025 alpha:0.98] setFill];
            NSRectFill(panel);
            [[NSColor colorWithCalibratedRed:red * 0.68 + 0.10
                                        green:green * 0.68 + 0.10
                                         blue:blue_component * 0.68 + 0.10
                                        alpha:1.0] setFill];
            NSRectFill(NSInsetRect(panel, 3.0, 3.0));
            [[NSColor colorWithCalibratedWhite:0.055 alpha:1.0] setFill];
            NSRectFill(NSInsetRect(panel, 7.0, 7.0));
            [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] setFill];
            NSRectFill(NSMakeRect(NSMinX(panel) + 9.0, NSMinY(panel) + 10.0,
                                  NSWidth(panel) - 18.0, 1.0));
            draw_fe8_outlined_text(
                state->forecast_font_white, state->forecast_font_ink,
                state->forecast_glyph_widths, guild_text,
                NSMakePoint(NSMidX(panel) - guild_width * 0.5,
                            NSMinY(panel) + 35.0), guild_scale
            );
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths, phase_text,
                NSMakePoint(NSMidX(panel) - phase_width * 0.5,
                            NSMinY(panel) + 78.0), phase_scale
            );
            [NSGraphicsContext restoreGraphicsState];
        }
    }

    if (state->game_over.has_value())
    {
        // This is deliberately not a normal phase card. Keep the gameplay
        // scene visible beneath a heavy, hostile screen treatment so the
        // player can still read where the final confrontation happened.
        const std::uint64_t tick = state->game_over->tick;
        const CGFloat pulse = 0.5 + 0.5 * std::sin(
            static_cast<double>(tick) * (2.0 * M_PI / 42.0)
        );
        const CGFloat impact = std::clamp(static_cast<CGFloat>(tick) / 22.0, 0.0, 1.0);
        const CGFloat slam = 1.0 - std::pow(1.0 - impact, 3.0);
        // Expand from the tactical board into the entire monitor content
        // area. This removes the sidebar for GAME OVER rather than treating
        // it as another small sidebar card.
        const NSRect full_area = self.bounds;
        const NSRect game_area = NSMakeRect(
            NSMinX(_board) + (NSMinX(full_area) - NSMinX(_board)) * slam,
            NSMinY(_board) + (NSMinY(full_area) - NSMinY(_board)) * slam,
            NSWidth(_board) + (NSWidth(full_area) - NSWidth(_board)) * slam,
            NSHeight(_board) + (NSHeight(full_area) - NSHeight(_board)) * slam
        );

        [NSGraphicsContext saveGraphicsState];
        CGContextRef game_over_context = [[NSGraphicsContext currentContext] CGContext];
        CGContextClipToRect(game_over_context, NSRectToCGRect(game_area));
        CGContextSetShouldAntialias(game_over_context, false);

        [[NSColor colorWithCalibratedRed:0.035 green:0.0 blue:0.015 alpha:0.74] setFill];
        NSRectFill(game_area);

        // Jagged red diagonal impact streaks: they scroll continuously after
        // the initial slam instead of leaving a dead static overlay.
        const CGFloat streak_spacing = std::max<CGFloat>(16.0, _cell_pixels * 0.72);
        const CGFloat streak_offset = std::fmod(static_cast<CGFloat>(tick) * 2.0, streak_spacing);
        for (CGFloat diagonal = -NSHeight(game_area) + streak_offset;
             diagonal < NSWidth(game_area) + NSHeight(game_area);
             diagonal += streak_spacing)
        {
            const CGFloat strength = 0.11 + 0.16 * pulse;
            [[NSColor colorWithCalibratedRed:0.84 green:0.035 blue:0.06 alpha:strength] setFill];
            NSBezierPath* stripe = [NSBezierPath bezierPath];
            [stripe moveToPoint:NSMakePoint(NSMinX(game_area) + diagonal, NSMinY(game_area))];
            [stripe lineToPoint:NSMakePoint(NSMinX(game_area) + diagonal + _cell_pixels * 0.22,
                                            NSMinY(game_area))];
            [stripe lineToPoint:NSMakePoint(NSMinX(game_area) + diagonal + NSHeight(game_area) + _cell_pixels * 0.22,
                                            NSMaxY(game_area))];
            [stripe lineToPoint:NSMakePoint(NSMinX(game_area) + diagonal + NSHeight(game_area),
                                            NSMaxY(game_area))];
            [stripe closePath];
            [stripe fill];
        }

        const CGFloat title_box_width = std::min<CGFloat>(NSWidth(game_area) * 0.82, 760.0);
        const CGFloat title_box_height = std::min<CGFloat>(
            NSHeight(game_area) * 0.62, std::max<CGFloat>(_cell_pixels * 4.6, 204.0)
        );
        const CGFloat title_y = NSMidY(game_area) - title_box_height * 0.5;
        const NSRect title_box = NSMakeRect(
            NSMidX(game_area) - title_box_width * 0.5,
            title_y,
            title_box_width,
            title_box_height
        );
        // Keep the entire title visible even on its first presentation frame.
        // The oversized impact comes from a brief controlled shake and pulse,
        // not by sliding half the graphic beyond the left board edge.
        const CGFloat shake = tick < 28 ? ((tick & 1U) == 0U ? 4.0 : -4.0) * (1.0 - slam) : 0.0;
        const NSRect landed_box = NSOffsetRect(title_box, shake, 0.0);

        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.98] setFill];
        NSRectFill(landed_box);
        [[NSColor colorWithCalibratedRed:0.78 green:0.045 blue:0.065 alpha:1.0] setFill];
        NSRectFill(NSInsetRect(landed_box, 4.0, 4.0));
        [[NSColor colorWithCalibratedRed:0.19 green:0.005 blue:0.012 alpha:1.0] setFill];
        NSRectFill(NSInsetRect(landed_box, 9.0, 9.0));
        [[NSColor colorWithCalibratedRed:1.0 green:0.25 + 0.12 * pulse blue:0.28 alpha:0.95] setFill];
        NSRectFill(NSMakeRect(NSMinX(landed_box) + 12.0, NSMinY(landed_box) + 12.0,
                              NSWidth(landed_box) - 24.0, 2.0));

        const auto bitmap_width = [&](NSString* text)
        {
            CGFloat width = 0.0;
            for (NSUInteger index = 0; index < text.length; ++index)
            {
                const unichar character = [text characterAtIndex:index];
                width += state->forecast_glyph_widths[
                    character < state->forecast_glyph_widths.size() ? character : '?'
                ];
            }
            return width;
        };
        const CGFloat game_scale = std::clamp(
            (NSWidth(landed_box) - 48.0) / std::max<CGFloat>(1.0, bitmap_width(@"GAME")),
            1.8, 4.60
        );
        const CGFloat over_scale = std::clamp(
            (NSWidth(landed_box) - 48.0) / std::max<CGFloat>(1.0, bitmap_width(@"OVER")),
            1.8, 4.60
        );
        const CGFloat game_width = bitmap_width(@"GAME") * game_scale;
        const CGFloat over_width = bitmap_width(@"OVER") * over_scale;
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths, @"GAME",
            NSMakePoint(NSMidX(landed_box) - game_width * 0.5,
                        NSMinY(landed_box) + 24.0), game_scale
        );
        draw_fe8_outlined_text(
            state->forecast_font_white, state->forecast_font_ink,
            state->forecast_glyph_widths, @"OVER",
            NSMakePoint(NSMidX(landed_box) - over_width * 0.5,
                        NSMinY(landed_box) + 31.0 + 18.0 * game_scale), over_scale
        );
        [NSGraphicsContext restoreGraphicsState];
    }
}
@end

void create_monitor_window(fe_tiles::MapMonitor::Impl* state)
{
    if (state->window != nil)
    {
        [state->window makeKeyAndOrderFront:nil];
        return;
    }

    const NSRect frame = NSMakeRect(0, 0, state->options.width, state->options.height);
    state->window = [[NSWindow alloc]
        initWithContentRect:frame
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                   NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
        backing:NSBackingStoreBuffered
        defer:NO];
    state->window.title = [NSString stringWithUTF8String:state->options.title.c_str()];
    state->window.minSize = NSMakeSize(760, 520);
    state->view = [[FE8MapMonitorView alloc] initWithFrame:frame state:state];
    state->window.contentView = state->view;
    [state->window makeKeyAndOrderFront:nil];
    [state->window makeFirstResponder:state->view];
    [NSApp activateIgnoringOtherApps:YES];

    // The monitor owns a 60 Hz display clock even if its caller elects to
    // tick AnimationRenderer elsewhere: phase intros are monitor-owned UI.
    __weak NSView* weak_view = state->view;
    fe_tiles::AnimationRenderer* renderer = state->renderer;
    state->timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                    repeats:YES
                                                      block:^(NSTimer* timer)
    {
        if (state->options.tick_renderer_at_60hz)
        {
            ++state->idle_tick;
            bool advance_combat = true;
            if (renderer->is_presenting_combat())
            {
                state->battle_tick_accumulator += state->options.battle_animation_speed;
                advance_combat = state->battle_tick_accumulator >= 1.0;
                if (advance_combat)
                {
                    state->battle_tick_accumulator -= 1.0;
                }
            }
            else
            {
                state->battle_tick_accumulator = 0.0;
            }
            renderer->tick_60hz(advance_combat);
            if (state->frame_callback)
            {
                state->frame_callback();
            }
        }
        if (state->phase_intro.has_value())
        {
            ++state->phase_intro->tick;
            if (state->phase_intro->tick >= 68)
            {
                state->phase_intro.reset();
            }
        }
        if (state->game_over.has_value())
        {
            ++state->game_over->tick;
        }
        [weak_view setNeedsDisplay:YES];
    }];
}

@interface FE8MapMonitorDelegate : NSObject <NSApplicationDelegate>
{
    void* _state;
}
- (instancetype)initWithState:(void*)state;
@end

@implementation FE8MapMonitorDelegate
- (instancetype)initWithState:(void*)state
{
    self = [super init];
    if (self)
    {
        _state = state;
    }
    return self;
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    auto* state = static_cast<fe_tiles::MapMonitor::Impl*>(_state);
    create_monitor_window(state);
}
@end

namespace fe_tiles
{
MapMonitor::MapMonitor(const maps::MapRecipe& recipe, AnimationRenderer& renderer)
    : MapMonitor(recipe, renderer, Options{})
{
}

MapMonitor::MapMonitor(const maps::MapRecipe& recipe,
                       AnimationRenderer& renderer,
                       Options options)
    : impl_(std::make_unique<Impl>(recipe, renderer, std::move(options)))
{
}

MapMonitor::~MapMonitor()
{
    close();
}

void MapMonitor::open()
{
    [NSApplication sharedApplication];
    create_monitor_window(impl_.get());
}

void MapMonitor::run()
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        FE8MapMonitorDelegate* delegate = [[FE8MapMonitorDelegate alloc] initWithState:impl_.get()];
        NSApp.delegate = delegate;
        [NSApp run];
    }
}

void MapMonitor::close()
{
    if (!impl_)
    {
        return;
    }
    [impl_->timer invalidate];
    impl_->timer = nil;
    [impl_->window close];
    impl_->view = nil;
    impl_->window = nil;
}

bool MapMonitor::is_open() const
{
    return impl_ != nullptr && impl_->window != nil && impl_->window.isVisible;
}

void MapMonitor::set_map(const maps::MapRecipe& recipe)
{
    impl_->require_compatible(recipe);
    impl_->recipe = recipe;
    impl_->rebuild_canvas();
    request_redraw();
}

void MapMonitor::request_redraw()
{
    [impl_->view setNeedsDisplay:YES];
}

void MapMonitor::set_cursor(const std::vector<int>& coordinate)
{
    if (coordinate.empty())
    {
        clear_cursor();
        return;
    }
    if (coordinate.size() != 2)
    {
        throw std::invalid_argument("Monitor cursor coordinate must be {x, y}.");
    }
    const int x = coordinate[0];
    const int y = coordinate[1];
    if (x < 0 || y < 0 || x >= impl_->recipe.columns() || y >= impl_->recipe.rows())
    {
        throw std::out_of_range("Monitor cursor coordinate is outside the map.");
    }
    impl_->cursor = std::make_pair(x, y);
    request_redraw();
}

void MapMonitor::clear_cursor()
{
    impl_->cursor.reset();
    request_redraw();
}

void MapMonitor::show_route_arrows(const std::vector<std::vector<int>>& route)
{
    std::vector<std::pair<int, int>> checked;
    checked.reserve(route.size());

    for (const std::vector<int>& coordinate : route)
    {
        if (coordinate.size() != 2)
        {
            throw std::invalid_argument("Route coordinates must have the form {x, y}.");
        }

        const int x = coordinate[0];
        const int y = coordinate[1];
        if (x < 0 || y < 0 || x >= impl_->recipe.columns() || y >= impl_->recipe.rows())
        {
            throw std::out_of_range("Route coordinate is outside the monitor map.");
        }
        checked.emplace_back(x, y);
    }

    impl_->route_arrows = {std::move(checked)};
    request_redraw();
}

void MapMonitor::show_route_arrows(
    const std::vector<std::vector<std::vector<int>>>& routes)
{
    std::vector<std::vector<std::pair<int, int>>> checked_routes;
    checked_routes.reserve(routes.size());

    for (const std::vector<std::vector<int>>& route : routes)
    {
        std::vector<std::pair<int, int>> checked;
        checked.reserve(route.size());

        for (const std::vector<int>& coordinate : route)
        {
            if (coordinate.size() != 2)
            {
                throw std::invalid_argument("Route coordinates must have the form {x, y}.");
            }

            const int x = coordinate[0];
            const int y = coordinate[1];
            if (x < 0 || y < 0 || x >= impl_->recipe.columns() || y >= impl_->recipe.rows())
            {
                throw std::out_of_range("Route coordinate is outside the monitor map.");
            }
            checked.emplace_back(x, y);
        }

        if (!checked.empty())
        {
            checked_routes.push_back(std::move(checked));
        }
    }

    impl_->route_arrows = std::move(checked_routes);
    request_redraw();
}

void MapMonitor::clear_route_arrows()
{
    impl_->route_arrows.clear();
    request_redraw();
}

bool MapMonitor::route_arrows_visible() const
{
    return !impl_->route_arrows.empty();
}

void MapMonitor::on_key(std::function<void(char)> callback)
{
    impl_->key_callback = std::move(callback);
}

void MapMonitor::on_frame(std::function<void()> callback)
{
    impl_->frame_callback = std::move(callback);
}

void MapMonitor::set_battle_animation_speed(double speed)
{
    impl_->options.battle_animation_speed = std::clamp(speed, 0.05, 1.0);
    impl_->battle_tick_accumulator = 0.0;
}

double MapMonitor::battle_animation_speed() const
{
    return impl_->options.battle_animation_speed;
}

void MapMonitor::show_battle_forecast(const Entity& attacker,
                                      const Entity& defender,
                                      const CombatInfo& attacker_combat,
                                      const CombatInfo& defender_combat)
{
    Impl::BattleForecast forecast;
    forecast.attacker_name = attacker.name;
    forecast.defender_name = defender.name;
    forecast.attacker_hp = attacker.stats.HP;
    forecast.attacker_max_hp = attacker.ogstats.HP;
    forecast.defender_hp = defender.stats.HP;
    forecast.defender_max_hp = defender.ogstats.HP;
    forecast.attacker_weapon = equipped_item(attacker);
    forecast.defender_weapon = equipped_item(defender);
    forecast.attacker_combat = attacker_combat;
    forecast.defender_combat = defender_combat;
    impl_->battle_forecast = std::move(forecast);
    request_redraw();
}

void MapMonitor::clear_battle_forecast()
{
    impl_->battle_forecast.reset();
    request_redraw();
}

bool MapMonitor::battle_forecast_visible() const
{
    return impl_->battle_forecast.has_value();
}

void MapMonitor::show_phase_intro(const std::string& guild_name, GuildColor color)
{
    if (guild_name.empty())
    {
        throw std::invalid_argument("Phase intro needs a non-empty guild name.");
    }
    impl_->phase_intro = Impl::PhaseIntro{guild_name, color, 0};
    impl_->phase_dialogue = Impl::PhaseDialogue{guild_name, color};
    request_redraw();
}

void MapMonitor::clear_phase_intro()
{
    impl_->phase_intro.reset();
    impl_->phase_dialogue.reset();
    request_redraw();
}

bool MapMonitor::phase_intro_visible() const
{
    return impl_->phase_intro.has_value();
}

void MapMonitor::show_game_over()
{
    impl_->battle_forecast.reset();
    impl_->phase_intro.reset();
    impl_->phase_dialogue.reset();
    impl_->game_over = Impl::GameOver{};
    request_redraw();
}

void MapMonitor::clear_game_over()
{
    impl_->game_over.reset();
    request_redraw();
}

bool MapMonitor::game_over_visible() const
{
    return impl_->game_over.has_value();
}
}
