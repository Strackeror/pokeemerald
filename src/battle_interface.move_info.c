#include "global.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "battle.h"
#include "battle_ai_util.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_main.h"
#include "battle_message.h"
#include "battle_util.h"
#include "constants/battle.h"
#include "constants/pokemon.h"
#include "data.h"
#include "decompress.h"
#include "graphics.h"
#include "menu.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_summary_screen.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"


EWRAM_DATA struct {
    bool8 loaded: 1;
    bool8 active: 1;
    u8 selectedDoubleMon: 1;
    u8 buttonPromptSprites[2];
    u8 window;
} sBattleMenuState;


static void SpriteCB_BattleInfoSystem_ButtonPromptWin(struct Sprite *sprite);
static void SpriteCB_BattleInfoSystem_InfoIcon(struct Sprite *sprite);

#define ABILITY_POP_UP_TAG 0xD723
static const u16 sAbilityPopUpPalette[] =
    INCBIN_U16("graphics/battle_interface/ability_pop_up.gbapal");
static const struct SpritePalette sSpritePalette_AbilityPopUp = {
    sAbilityPopUpPalette,
    ABILITY_POP_UP_TAG,
};

#define TAG_BIS_BUTTONS 0xD722
static const struct OamData sOamData_BattleInfoSystem_ButtonPrompt = {
    .y = 0,
    .affineMode = 0,
    .objMode = 0,
    .mosaic = 0,
    .bpp = 0,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const struct SpriteTemplate sSpriteTemplate_BattleInfoSystem_ButtonPromptWindow = {
    .tileTag = TAG_BIS_BUTTONS,
    .paletteTag = ABILITY_POP_UP_TAG,
    .oam = &sOamData_BattleInfoSystem_ButtonPrompt,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_BattleInfoSystem_ButtonPromptWin
};
static const u8 sBattleInfoSystem_ButtonPromptWindowGfx[] =
    INCBIN_U8("graphics/battle_interface/bis_la.4bpp");

static const struct SpriteSheet sSpriteSheet_BattleInfoSystem_ButtonPromptWindow = {
    sBattleInfoSystem_ButtonPromptWindowGfx,
    sizeof(sBattleInfoSystem_ButtonPromptWindowGfx),
    TAG_BIS_BUTTONS
};

#define TAG_BattleInfoSystem_INFO_ICON 30020
static const struct OamData sOamData_BattleInfoSystem_InfoIcon = {
    .y = 0,
    .affineMode = 0,
    .objMode = 0,
    .mosaic = 0,
    .bpp = 0,
    .shape = SPRITE_SHAPE(16x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};
static const struct SpriteTemplate sSpriteTemplate_BattleInfoSystem_InfoIcon = {
    .tileTag = TAG_BattleInfoSystem_INFO_ICON,
    .paletteTag = ABILITY_POP_UP_TAG,
    .oam = &sOamData_BattleInfoSystem_InfoIcon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_BattleInfoSystem_InfoIcon
};
static const u8 sBattleInfoSystem_ButtonPromptInfoIcon[] =
    INCBIN_U8("graphics/battle_interface/bis_info_icon.4bpp");
static const struct SpriteSheet sSpriteSheet_BattleInfoSystem_InfoIcon = {
    sBattleInfoSystem_ButtonPromptInfoIcon,
    sizeof(sBattleInfoSystem_ButtonPromptInfoIcon),
    TAG_BattleInfoSystem_INFO_ICON
};

#define BattleInfoSystem_BUTTON_PROMPT_X_F 10
#define BattleInfoSystem_BUTTON_PROMPT_X_0 -10
#define BattleInfoSystem_BUTTON_PROMPT_Y ((IsDoubleBattle()) ? 99 : 99)

#define BattleInfoSystem_BUTTON_PROMPT_WIN_X_F (BattleInfoSystem_BUTTON_PROMPT_X_F + 4)
#define BattleInfoSystem_BUTTON_PROMPT_WIN_X_0 (BattleInfoSystem_BUTTON_PROMPT_X_0 + 5)
#define BattleInfoSystem_BUTTON_PROMPT_WIN_Y (BattleInfoSystem_BUTTON_PROMPT_Y - 3)

#define sHideABI data[1]

static void TryAddBattleInfoSystem_ButtonPromptItemSprites(void)
{
    LoadSpritePalette(&sSpritePalette_AbilityPopUp);

    // icon
    if (GetSpriteTileStartByTag(TAG_BattleInfoSystem_INFO_ICON) == 0xFFFF)
        LoadSpriteSheet(&sSpriteSheet_BattleInfoSystem_InfoIcon);

    if (sBattleMenuState.buttonPromptSprites[0] == MAX_SPRITES)
    {
        sBattleMenuState.buttonPromptSprites[0] =
            CreateSprite(&sSpriteTemplate_BattleInfoSystem_InfoIcon,
                BattleInfoSystem_BUTTON_PROMPT_X_0,
                BattleInfoSystem_BUTTON_PROMPT_Y,
                5);
        gSprites[sBattleMenuState.buttonPromptSprites[0]].sHideABI = FALSE; // restore
    }

    // window
    if (GetSpriteTileStartByTag(TAG_BIS_BUTTONS) == 0xFFFF)
        LoadSpriteSheet(&sSpriteSheet_BattleInfoSystem_ButtonPromptWindow);

    if (sBattleMenuState.buttonPromptSprites[1] == MAX_SPRITES)
    {
        sBattleMenuState.buttonPromptSprites[1] =
            CreateSprite(&sSpriteTemplate_BattleInfoSystem_ButtonPromptWindow,
                BattleInfoSystem_BUTTON_PROMPT_WIN_X_0,
                BattleInfoSystem_BUTTON_PROMPT_WIN_Y,
                5);
        gSprites[sBattleMenuState.buttonPromptSprites[0]].sHideABI = FALSE; // restore
    }
}

static void DestroyBattleInfoSystem_ButtonPromptWinGfx(struct Sprite *sprite)
{
    FreeSpriteTilesByTag(TAG_BIS_BUTTONS);
    FreeSpritePaletteByTag(ABILITY_POP_UP_TAG);
    DestroySprite(sprite);
    sBattleMenuState.loaded = FALSE;
    sBattleMenuState.buttonPromptSprites[1] = MAX_SPRITES;
}

static void DestroyBattleInfoSystem_ButtonPromptGfx(struct Sprite *sprite)
{
    FreeSpriteTilesByTag(TAG_BattleInfoSystem_INFO_ICON);
    FreeSpritePaletteByTag(ABILITY_POP_UP_TAG);
    DestroySprite(sprite);
    sBattleMenuState.buttonPromptSprites[0] = MAX_SPRITES;
}

static void SpriteCB_BattleInfoSystem_ButtonPromptWin(struct Sprite *sprite)
{
    if (sprite->sHideABI)
    {
        if (sprite->x != BattleInfoSystem_BUTTON_PROMPT_WIN_X_0)
            sprite->x--;

        if (sprite->x == BattleInfoSystem_BUTTON_PROMPT_WIN_X_0)
            DestroyBattleInfoSystem_ButtonPromptWinGfx(sprite);
    }
    else
    {
        if (sprite->x != BattleInfoSystem_BUTTON_PROMPT_WIN_X_F)
            sprite->x++;
    }
}

static void SpriteCB_BattleInfoSystem_InfoIcon(struct Sprite *sprite)
{
    if (sprite->sHideABI)
    {
        if (sprite->x != BattleInfoSystem_BUTTON_PROMPT_X_0)
            sprite->x--;

        if (sprite->x == BattleInfoSystem_BUTTON_PROMPT_X_0)
            DestroyBattleInfoSystem_ButtonPromptGfx(sprite);
    }
    else
    {
        if (sprite->x != BattleInfoSystem_BUTTON_PROMPT_X_F)
            sprite->x++;
    }
}

static void TryHideOrRestoreBattleInfoSystem_ButtonPrompt(u8 caseId)
{
    if (sBattleMenuState.buttonPromptSprites[0] == MAX_SPRITES)
        return;

    switch (caseId)
    {
    case 0: // hide
        if (sBattleMenuState.buttonPromptSprites[0] != MAX_SPRITES)
            gSprites[sBattleMenuState.buttonPromptSprites[0]].sHideABI = TRUE; // hide
        if (sBattleMenuState.buttonPromptSprites[1] != MAX_SPRITES)
            gSprites[sBattleMenuState.buttonPromptSprites[1]].sHideABI = TRUE; // hide
        break;
    case 1: // restore
        if (sBattleMenuState.buttonPromptSprites[0] != MAX_SPRITES)
            gSprites[sBattleMenuState.buttonPromptSprites[0]].sHideABI = FALSE; // restore
        if (sBattleMenuState.buttonPromptSprites[1] != MAX_SPRITES)
            gSprites[sBattleMenuState.buttonPromptSprites[1]].sHideABI = FALSE; // restore
        break;
    }
}

void TryHideBattleInfoSystem_ButtonPrompt(void)
{
    TryHideOrRestoreBattleInfoSystem_ButtonPrompt(0);
}

void TryRestoreBattleInfoSystem_ButtonPrompt(void)
{
    if (!sBattleMenuState.loaded) {
        sBattleMenuState.loaded = TRUE;
        sBattleMenuState.buttonPromptSprites[0] = MAX_SPRITES;
        sBattleMenuState.buttonPromptSprites[1] = MAX_SPRITES;
        TryAddBattleInfoSystem_ButtonPromptItemSprites();
    }
    else
    {
        TryHideOrRestoreBattleInfoSystem_ButtonPrompt(1);
    }
}


static u8 GetShowPokemon()
{
    if (!IsDoubleBattle()) 
        return GetBattlerPosition(B_POSITION_OPPONENT_LEFT);
    if (sBattleMenuState.selectedDoubleMon) 
        return GetBattlerPosition(B_POSITION_OPPONENT_RIGHT);
    return GetBattlerPosition(B_POSITION_OPPONENT_LEFT);
}

enum Speed {
    TIE,
    FIRST,
    SECOND,
};

struct PotentialDamage {
    u16 min;
    u16 max;
    enum Speed speed;
};

struct PotentialDamage CalcPotentialDamage(u16 move) {
    u8 targetId = GetShowPokemon();
    u8 moveType;
    GET_MOVE_TYPE(move, moveType);

    u32 dmg = CalculateMoveDamage(move, gActiveBattler, targetId, moveType, 0, FALSE, FALSE, FALSE);
    struct Pokemon* target = &gEnemyParty[gBattlerPartyIndexes[targetId]];

    u32 atkSpeed = GetBattlerTotalSpeedStat(gActiveBattler);
    u32 defSpeed = GetBattlerTotalSpeedStat(targetId);
    enum Speed speed;

    if (atkSpeed > defSpeed)
        speed = FIRST;
    else if (atkSpeed < defSpeed)
        speed = SECOND;
    else
        speed = TIE;

    u32 minDmg = dmg * 86 / 100;

    u32 percentage = dmg * 1000 / target->maxHP;
    struct PotentialDamage ret = {
        .min = minDmg * 1000 / target->maxHP,
        .max = dmg * 1000 / target->maxHP,
        .speed = speed,
    };
    return ret;
}

#define MOVE_DESCRIPTION_WINDOW_ID = 24;

static const struct WindowTemplate sDescriptionWindowTemplate = {

    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 55,
    .width = 18,
    .height = 4,
    .paletteNum = 5,
    .baseBlock = 0x0350,
};

static const u8 sText_Power[] = _("Pow:");
static const u8 sText_Accuracy[] = _(" Ac:");
static const u8 sText_ThreeDashes[] = _("---");

static const u8 sText_1st[] = _("1st");
static const u8 sText_2nd[] = _("2nd");
static const u8 sText_Eq[] = _(" Eq");
static const u8 sText_FormatCalc[] = _("{STR_VAR_1} {STR_VAR_2}%-{STR_VAR_3}%");


void MoveSelectionDisplayMoveData(void)
{
    struct ChooseMoveStruct *moveInfo =
        (struct ChooseMoveStruct *)(&gBattleResources->bufferA[gActiveBattler][4]);
    u16 move = moveInfo->moves[gMoveSelectionCursor[gActiveBattler]];
    u8 *txtPtr;

    // Power
    u8 power = gBattleMoves[move].power;
    txtPtr = StringCopy(gDisplayedStringBattle, sText_Power);
    if (power)
        ConvertIntToDecimalStringN(txtPtr, power, STR_CONV_MODE_RIGHT_ALIGN, 3);
    else
        StringCopy(txtPtr, sText_ThreeDashes);
    BattlePutTextOnWindow(gDisplayedStringBattle, 7);

    // Accuracy
    u8 accuracy = gBattleMoves[move].accuracy;
    txtPtr = StringCopy(gDisplayedStringBattle, sText_Accuracy);
    if (accuracy) 
        ConvertIntToDecimalStringN(txtPtr, accuracy, STR_CONV_MODE_RIGHT_ALIGN, 3);
    else
        StringCopy(txtPtr, sText_ThreeDashes);
    BattlePutTextOnWindow(gDisplayedStringBattle, 9);


    // Damage Calcs
    struct PotentialDamage dmg = CalcPotentialDamage(move);
    switch (dmg.speed) {
        case TIE:
            StringCopy(gStringVar1, sText_Eq);
            break;
        case FIRST:
            StringCopy(gStringVar1, sText_1st);
            break;
        case SECOND:
            StringCopy(gStringVar1, sText_2nd);
            break;
    }
    if (gBattleMoves[move].split != SPLIT_STATUS)
    {
            ConvertIntToDecimalStringN(gStringVar2, dmg.min / 10, STR_CONV_MODE_RIGHT_ALIGN, 3);
            ConvertIntToDecimalStringN(gStringVar3, dmg.max / 10, STR_CONV_MODE_RIGHT_ALIGN, 3);
            StringExpandPlaceholders(gDisplayedStringBattle, sText_FormatCalc);
            BattlePutTextOnWindow(gDisplayedStringBattle, 10);
    }
    else
    {
            BattlePutTextOnWindow(gStringVar1, 10);
    }

    // Description
    if (!sBattleMenuState.active) {
        sBattleMenuState.window = AddWindow(&sDescriptionWindowTemplate);
    }
    u8 window = sBattleMenuState.window;
    struct TextPrinterTemplate descriptionTemplate = {
        .currentChar = gMoveDescriptionPointers[move - 1],
        .windowId = window,
        .fontId = 1,
        .x = 0,
        .y = 1,
        .currentX = 0,
        .currentY = 1,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = TEXT_DYNAMIC_COLOR_4,
        .bgColor = TEXT_DYNAMIC_COLOR_5,
        .shadowColor = TEXT_DYNAMIC_COLOR_6,
    };
    FillWindowPixelBuffer(window, PIXEL_FILL(0xE));
    AddTextPrinter(&descriptionTemplate, 0, NULL);
    CopyWindowToVram(window, 3);
    PutWindowTilemap(window);
}

enum
{
    CONTINUE,
    BREAK,
    REFRESH,
};


u8 HandleInputMoveInfo()
{
    if (sBattleMenuState.active)
    {
        if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            sBattleMenuState.active = FALSE;
            TryRestoreBattleInfoSystem_ButtonPrompt();
            RemoveWindow(sBattleMenuState.window);
            if (IsDoubleBattle()) 
                gSprites[gBattlerSpriteIds[GetShowPokemon()]].callback = SpriteCb_HideAsMoveTarget;
            return REFRESH;
        }
        if (IsDoubleBattle() && JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
        {
            gSprites[gBattlerSpriteIds[GetShowPokemon()]].callback = SpriteCb_HideAsMoveTarget;
            sBattleMenuState.selectedDoubleMon = !sBattleMenuState.selectedDoubleMon;
            gSprites[gBattlerSpriteIds[GetShowPokemon()]].callback = SpriteCb_ShowAsMoveTarget;
            MoveSelectionDisplayMoveData();
        }
    }
    else
    {
        TryRestoreBattleInfoSystem_ButtonPrompt();
        if (JOY_NEW(A_BUTTON) && JOY_HELD(L_BUTTON))
        {
            MoveSelectionDisplayMoveData();
            TryHideBattleInfoSystem_ButtonPrompt();
            sBattleMenuState.active = TRUE;
            TypeInfoDestroySprites();
            if (IsDoubleBattle())
                gSprites[gBattlerSpriteIds[GetShowPokemon()]].callback = SpriteCb_ShowAsMoveTarget;
        }
        if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            TryHideBattleInfoSystem_ButtonPrompt();
        }
    }

    switch (sBattleMenuState.active)
    {
    case 1:
        return BREAK;
    default:
    case 0:
        return CONTINUE;
    }
}
