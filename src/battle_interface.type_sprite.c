#include "global.h"
#include "graphics.h"
#include "palette.h"
#include "decompress.h"
#include "pokemon_summary_screen.h"
#include "sprite.h"

EWRAM_DATA u8 gBattleInfoMoveTypeSpriteId = MAX_SPRITES;
EWRAM_DATA u8 gBattleInfoMoveSplitSpriteId = MAX_SPRITES;

#define TAG_MOVE_TYPES 30002
static const u8 sMoveTypeToOamPaletteNum[] =
{
    [TYPE_NORMAL] = 13,
    [TYPE_FIGHTING] = 13,
    [TYPE_FLYING] = 14,
    [TYPE_POISON] = 14,
    [TYPE_GROUND] = 13,
    [TYPE_ROCK] = 13,
    [TYPE_BUG] = 15,
    [TYPE_GHOST] = 14,
    [TYPE_STEEL] = 13,
    [TYPE_MYSTERY] = 15,
    [TYPE_FIRE] = 13,
    [TYPE_WATER] = 14,
    [TYPE_GRASS] = 15,
    [TYPE_ELECTRIC] = 13,
    [TYPE_PSYCHIC] = 14,
    [TYPE_ICE] = 14,
    [TYPE_DRAGON] = 15,
    [TYPE_DARK] = 13,
    [TYPE_FAIRY] = 14,
};

static const struct CompressedSpritePalette sType_Palette =
{
    gMoveTypes_Pal, TAG_MOVE_TYPES
};

static void CreateTypeIconSprite(u8 moveType)
{
    struct Sprite* sprite;
    if (GetSpriteTileStartByTag(TAG_MOVE_TYPES) == 0xFFFF) {
        gBattleInfoMoveTypeSpriteId = MAX_SPRITES;
    }

    if (gBattleInfoMoveTypeSpriteId == MAX_SPRITES) {
        LoadCompressedPalette(gMoveTypes_Pal, 0x1d0, 0x60);
        LoadCompressedSpriteSheet(&sSpriteSheet_MoveTypes);
        gBattleInfoMoveTypeSpriteId = CreateSprite(&sSpriteTemplate_MoveTypes, 184, 144, 5);
    }
    sprite = &gSprites[gBattleInfoMoveTypeSpriteId];
    sprite->oam.priority = 0;
    sprite->oam.paletteNum = sMoveTypeToOamPaletteNum[moveType];
    StartSpriteAnim(sprite, moveType);
}

static void DestroyTypeIconSprite()
{
    if (gBattleInfoMoveTypeSpriteId != MAX_SPRITES) {
        FreeSpriteTilesByTag(TAG_MOVE_TYPES);
        FreeSpritePaletteByTag(TAG_MOVE_TYPES);
        DestroySprite(&gSprites[gBattleInfoMoveTypeSpriteId]);
        gBattleInfoMoveTypeSpriteId = MAX_SPRITES;
    }
}

#define TAG_SPLIT_ICONS 30004
static const u16 sSplitIcons_Pal[] = INCBIN_U16("graphics/interface/split_icons.gbapal");
static const u32 sSplitIcons_Gfx[] = INCBIN_U32("graphics/interface/split_icons.4bpp.lz");
static const struct OamData sOamData_SplitIcons =
{
    .size = SPRITE_SIZE(16x16),
    .shape = SPRITE_SHAPE(16x16),
    .priority = 0,
};
static const struct CompressedSpriteSheet sSpriteSheet_SplitIcons =
{
    .data = sSplitIcons_Gfx,
    .size = 16*16*3/2,
    .tag = TAG_SPLIT_ICONS,
};
static const struct SpritePalette sSpritePal_SplitIcons =
{
    .data = sSplitIcons_Pal,
    .tag = TAG_SPLIT_ICONS
};
static const union AnimCmd sSpriteAnim_SplitIcon0[] =
{
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_SplitIcon1[] =
{
    ANIMCMD_FRAME(4, 0),
    ANIMCMD_END
};
static const union AnimCmd sSpriteAnim_SplitIcon2[] =
{
    ANIMCMD_FRAME(8, 0),
    ANIMCMD_END
};
static const union AnimCmd *const sSpriteAnimTable_SplitIcons[] =
{
    sSpriteAnim_SplitIcon0,
    sSpriteAnim_SplitIcon1,
    sSpriteAnim_SplitIcon2,
};
static const struct SpriteTemplate sSpriteTemplate_SplitIcons =
{
    .tileTag = TAG_SPLIT_ICONS,
    .paletteTag = TAG_SPLIT_ICONS,
    .oam = &sOamData_SplitIcons,
    .anims = sSpriteAnimTable_SplitIcons,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static void CreateMoveSplitSprite(u8 splitType)
{
    struct Sprite* sprite;
    if (GetSpriteTileStartByTag(TAG_SPLIT_ICONS) == 0xFFFF) {
        gBattleInfoMoveSplitSpriteId = MAX_SPRITES;
    }

    if (gBattleInfoMoveSplitSpriteId == MAX_SPRITES) {
        LoadCompressedSpriteSheet(&sSpriteSheet_SplitIcons);
        LoadSpritePalette(&sSpritePal_SplitIcons);
        gBattleInfoMoveSplitSpriteId = CreateSprite(&sSpriteTemplate_SplitIcons, 210, 144, 5);
    }
    sprite = &gSprites[gBattleInfoMoveSplitSpriteId];
    StartSpriteAnim(sprite, splitType);
}

static void DestroyMoveSplitSprite()
{
    if (gBattleInfoMoveSplitSpriteId != MAX_SPRITES) {
        FreeSpriteTilesByTag(TAG_SPLIT_ICONS);
        FreeSpritePaletteByTag(TAG_SPLIT_ICONS);
        DestroySprite(&gSprites[gBattleInfoMoveSplitSpriteId]);
        gBattleInfoMoveSplitSpriteId = MAX_SPRITES;
    }
}

void CreateTypeInfoSprites(u8 moveType, u8 splitType)
{
    CreateTypeIconSprite(moveType);
    CreateMoveSplitSprite(splitType);
}

void DestroyTypeInfoSprite()
{
    DestroyTypeIconSprite();
    DestroyMoveSplitSprite();
}
