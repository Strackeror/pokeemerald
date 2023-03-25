#include "global.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_message.h"
#include "battle_util.h"
#include "constants/abilities.h"
#include "constants/battle.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "decompress.h"
#include "graphics.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_summary_screen.h"
#include "sprite.h"
#include "string_util.h"

static EWRAM_DATA struct
{
    u8 typeSprite;
    u8 splitSprite;
    bool8 doubleActive : 1;
} sTypeSprite;

#define TAG_MOVE_TYPES 30002
static const u8 sMoveTypeToOamPaletteNum[] = {
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

static const struct CompressedSpritePalette sType_Palette = { gMoveTypes_Pal, TAG_MOVE_TYPES };

static void CreateTypeIconSprite(u8 moveType)
{
    struct Sprite *sprite;
    if (GetSpriteTileStartByTag(TAG_MOVE_TYPES) == 0xFFFF)
    {
        sTypeSprite.typeSprite = MAX_SPRITES;
    }

    if (sTypeSprite.typeSprite == MAX_SPRITES)
    {
        LoadCompressedPalette(gMoveTypes_Pal, 0x1d0, 0x60);
        LoadCompressedSpriteSheet(&sSpriteSheet_MoveTypes);
        sTypeSprite.typeSprite = CreateSprite(&sSpriteTemplate_MoveTypes, 203, 145, 5);
    }
    sprite = &gSprites[sTypeSprite.typeSprite];
    sprite->oam.priority = 0;
    sprite->oam.paletteNum = sMoveTypeToOamPaletteNum[moveType];
    StartSpriteAnim(sprite, moveType);
}

static void DestroyTypeIconSprite()
{
    if (sTypeSprite.typeSprite != MAX_SPRITES)
    {
        FreeSpriteTilesByTag(TAG_MOVE_TYPES);
        FreeSpritePaletteByTag(TAG_MOVE_TYPES);
        DestroySprite(&gSprites[sTypeSprite.typeSprite]);
        sTypeSprite.typeSprite = MAX_SPRITES;
    }
}

#define TAG_SPLIT_ICONS 30004
static const u16 sSplitIcons_Pal[] = INCBIN_U16("graphics/interface/split_icons.gbapal");
static const u32 sSplitIcons_Gfx[] = INCBIN_U32("graphics/interface/split_icons.4bpp.lz");
static const struct OamData sOamData_SplitIcons = {
    .size = SPRITE_SIZE(16x16),
    .shape = SPRITE_SHAPE(16x16),
    .priority = 0,
};
static const struct CompressedSpriteSheet sSpriteSheet_SplitIcons = {
    .data = sSplitIcons_Gfx,
    .size = 16 * 16 * 3 / 2,
    .tag = TAG_SPLIT_ICONS,
};
static const struct SpritePalette sSpritePal_SplitIcons = {
    .data = sSplitIcons_Pal,
    .tag = TAG_SPLIT_ICONS,
};
static const union AnimCmd sSpriteAnim_SplitIcon0[] = { ANIMCMD_FRAME(0, 0), ANIMCMD_END };
static const union AnimCmd sSpriteAnim_SplitIcon1[] = { ANIMCMD_FRAME(4, 0), ANIMCMD_END };
static const union AnimCmd sSpriteAnim_SplitIcon2[] = { ANIMCMD_FRAME(8, 0), ANIMCMD_END };
static const union AnimCmd *const sSpriteAnimTable_SplitIcons[] = {
    sSpriteAnim_SplitIcon0,
    sSpriteAnim_SplitIcon1,
    sSpriteAnim_SplitIcon2,
};
static const struct SpriteTemplate sSpriteTemplate_SplitIcons = {
    .tileTag = TAG_SPLIT_ICONS,
    .paletteTag = TAG_SPLIT_ICONS,
    .oam = &sOamData_SplitIcons,
    .anims = sSpriteAnimTable_SplitIcons,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static void CreateMoveSplitSprite(u8 splitType)
{
    struct Sprite *sprite;
    if (GetSpriteTileStartByTag(TAG_SPLIT_ICONS) == 0xFFFF)
    {
        sTypeSprite.splitSprite = MAX_SPRITES;
    }

    if (sTypeSprite.splitSprite == MAX_SPRITES)
    {
        LoadCompressedSpriteSheet(&sSpriteSheet_SplitIcons);
        LoadSpritePalette(&sSpritePal_SplitIcons);
        sTypeSprite.splitSprite = CreateSprite(&sSpriteTemplate_SplitIcons, 227, 145, 5);
    }
    sprite = &gSprites[sTypeSprite.splitSprite];
    StartSpriteAnim(sprite, splitType);
}

static void DestroyMoveSplitSprite()
{
    if (sTypeSprite.splitSprite != MAX_SPRITES)
    {
        FreeSpriteTilesByTag(TAG_SPLIT_ICONS);
        FreeSpritePaletteByTag(TAG_SPLIT_ICONS);
        DestroySprite(&gSprites[sTypeSprite.splitSprite]);
        sTypeSprite.splitSprite = MAX_SPRITES;
    }
}

#define TYPE_x0 0
#define TYPE_x0_25 5
#define TYPE_x0_50 10
#define TYPE_x1 20
#define TYPE_x2 40
#define TYPE_x4 80

static int GetTypeEffectivenessPoints(int move, int targetSpecies)
{
    u16 defType1, defType2, defAbility, moveType;
    int i = 0;
    int typePower = TYPE_x1;

    if (move == MOVE_NONE || move == 0xFFFF || gBattleMoves[move].power == 0)
        return 0;

    defType1 = gBaseStats[targetSpecies].type1;
    defType2 = gBaseStats[targetSpecies].type2;
    defAbility = gBaseStats[targetSpecies].abilities[0];
    moveType = gBattleMoves[move].type;

    if (defAbility == ABILITY_LEVITATE && moveType == TYPE_GROUND)
    {
        typePower = 0;
    }
    else
    {
        u32 typeEffectiveness1 = UQ_4_12_TO_INT(GetTypeModifier(moveType, defType1) * 2) * 5;
        u32 typeEffectiveness2 = UQ_4_12_TO_INT(GetTypeModifier(moveType, defType2) * 2) * 5;

        typePower = (typeEffectiveness1 * typePower) / 10;
        if (defType2 != defType1)
            typePower = (typeEffectiveness2 * typePower) / 10;

        if (defAbility == ABILITY_WONDER_GUARD && typeEffectiveness1 != 20 &&
            typeEffectiveness2 != 20)
            typePower = 0;
    }
    return typePower;
}

static const u8 sText_TypeEffectiveness_x0[] = _("{BIG_MULT_X}");
static const u8 sText_TypeEffectiveness_x0_25[] = _("{DOWN_ARROW}{DOWN_ARROW}");
static const u8 sText_TypeEffectiveness_x0_50[] = _("{DOWN_ARROW}");
static const u8 sText_TypeEffectiveness_x1[] = _("=");
static const u8 sText_TypeEffectiveness_x2[] = _("{UP_ARROW}");
static const u8 sText_TypeEffectiveness_x4[] = _("{UP_ARROW}{UP_ARROW}");

static void CopyStringMoveEffect(u8 *dest, u16 move, u16 target)
{
    if (gBattleMoves[move].split == SPLIT_STATUS)
    {
        StringCopy(dest, sText_TypeEffectiveness_x1);
        return;
    }

    u8 typePower = GetTypeEffectivenessPoints(move, target);
    switch (typePower)
    {
    case TYPE_x0:
        StringCopy(dest, sText_TypeEffectiveness_x0);
        break;
    case TYPE_x0_25:
        StringCopy(dest, sText_TypeEffectiveness_x0_25);
        break;
    case TYPE_x0_50:
        StringCopy(dest, sText_TypeEffectiveness_x0_50);
        break;
    case TYPE_x1:
        StringCopy(dest, sText_TypeEffectiveness_x1);
        break;
    case TYPE_x2:
        StringCopy(dest, sText_TypeEffectiveness_x2);
        break;
    case TYPE_x4:
        StringCopy(dest, sText_TypeEffectiveness_x4);
        break;
    }
}

static void ShowMoveTypeEffect(void)
{
    struct ChooseMoveStruct *moveInfo =
        (struct ChooseMoveStruct *)(&gBattleResources->bufferA[gActiveBattler][4]);
    u16 move = moveInfo->moves[gMoveSelectionCursor[gActiveBattler]];
    u8 partyId = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);

    u16 targetSpecies;
    if (!IsDoubleBattle())
    {
        targetSpecies = GetMonData(&gEnemyParty[gBattlerPartyIndexes[partyId]], MON_DATA_SPECIES);
    }
    else
    {
        if (!sTypeSprite.doubleActive)
        {
            BattlePutTextOnWindow(gText_EmptyString3, 10);
            return;
        }
        switch (GetBattlerPosition(gMultiUsePlayerCursor))
        {
        case B_POSITION_PLAYER_LEFT:
        case B_POSITION_PLAYER_RIGHT:
            targetSpecies = GetMonData(
                &gPlayerParty[gBattlerPartyIndexes[gMultiUsePlayerCursor]], MON_DATA_SPECIES);
            break;

        case B_POSITION_OPPONENT_LEFT:
        case B_POSITION_OPPONENT_RIGHT:
            targetSpecies = GetMonData(
                &gEnemyParty[gBattlerPartyIndexes[gMultiUsePlayerCursor]], MON_DATA_SPECIES);
            break;
        }
    }
    CopyStringMoveEffect(gDisplayedStringBattle, move, targetSpecies);
    BattlePutTextOnWindow(gDisplayedStringBattle, 10);
}

void TypeInfoDisplaySprites()
{

    struct ChooseMoveStruct *moveInfo =
        (struct ChooseMoveStruct *)(&gBattleResources->bufferA[gActiveBattler][4]);
    u8 type = gBattleMoves[moveInfo->moves[gMoveSelectionCursor[gActiveBattler]]].type;
    u8 split = gBattleMoves[moveInfo->moves[gMoveSelectionCursor[gActiveBattler]]].split;

    CreateTypeIconSprite(type);
    CreateMoveSplitSprite(split);
    ShowMoveTypeEffect();
}

void TypeInfoDestroySprites()
{
    sTypeSprite.doubleActive = FALSE;
    DestroyTypeIconSprite();
    DestroyMoveSplitSprite();
}

void TypeInfoUpdateDouble(bool8 active) {
    sTypeSprite.doubleActive = active;
    ShowMoveTypeEffect();
}
