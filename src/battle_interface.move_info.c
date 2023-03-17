#include "global.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_message.h"
#include "data.h"
#include "decompress.h"
#include "graphics.h"
#include "menu.h"
#include "palette.h"
#include "pokemon_summary_screen.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "window.h"


EWRAM_DATA struct {
    bool8 loaded: 1;
    bool8 active: 1;
    u8 buttonPromptSprites[2];
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

    // Description
    u8 window = AddWindow(&sDescriptionWindowTemplate);
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
    RemoveWindow(window);
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
            return REFRESH;
        }
    }
    else
    {
        TryRestoreBattleInfoSystem_ButtonPrompt();
        if (JOY_NEW(A_BUTTON) && JOY_HELD(L_BUTTON))
        {
            sBattleMenuState.active = TRUE;
            MoveSelectionDisplayMoveData();
            TryHideBattleInfoSystem_ButtonPrompt();
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