#ifdef TARGET_N3DS

#include "macros.h"

#include "gfx_3ds.h"
#include "gfx_3ds_menu.h"

C3D_RenderTarget *gTarget;
C3D_RenderTarget *gTargetRight;
float gSliderLevel;

struct gfx_configuration gfx_config = {false, false};

Gfx3DSMode gGfx3DSMode;
PrintConsole gConsole;

bool menu_mode;

u8 n3ds_model = 0;

static bool checkN3DS()
{
    bool isNew3DS = false;
    if (R_SUCCEEDED(APT_CheckNew3DS(&isNew3DS)))
        return isNew3DS;

    return false;
}

static void stop_top_screens()
{
    if (gTarget != NULL)
    {
        C3D_RenderTargetDelete(gTarget);
        gTarget = NULL;
    }
#ifdef ENABLE_N3DS_3D_MODE
    if (gTargetRight != NULL)
    {
        C3D_RenderTargetDelete(gTargetRight);
        gTargetRight = NULL;
    }
#endif
    C3D_Fini();
}

static void init_top_screens()
{
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    bool useAA = gfx_config.useAA;
    bool useWide =  gfx_config.useWide && n3ds_model != 3; // old 2DS does not support 800px

    u32 transferFlags =
        GX_TRANSFER_FLIP_VERT(0) |
        GX_TRANSFER_OUT_TILED(0) |
        GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8);

    if (useAA && !useWide)
        transferFlags |= GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_XY);
    else if (useAA && useWide)
        transferFlags |= GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_X);
    else
        transferFlags |= GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    int width = useAA || useWide ? 800 : 400;
    int height = useAA ? 480 : 240;

    gTarget = C3D_RenderTargetCreate(height, width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(gTarget, GFX_TOP, GFX_LEFT, transferFlags);

    if (!useWide)
    {
        gfxSetWide(false);
#ifdef ENABLE_N3DS_3D_MODE
        gTargetRight = C3D_RenderTargetCreate(height, width, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        C3D_RenderTargetSetOutput(gTargetRight, GFX_TOP, GFX_RIGHT, transferFlags);
        gfxSet3D(true);
#endif
    }
    else
    {
        gfxSet3D(false);
        gfxSetWide(true);
    }

    // used to determine scissoring
    if (!useAA && !useWide)
        gGfx3DSMode = GFX_3DS_MODE_NORMAL;
    else if (useAA && !useWide)
        gGfx3DSMode = GFX_3DS_MODE_AA_22;
    else if (!useAA && useWide)
        gGfx3DSMode = GFX_3DS_MODE_WIDE;
    else
        gGfx3DSMode = GFX_3DS_MODE_WIDE_AA_12;
}

void set_bottom_screen(bool enable)
{
    if (n3ds_model != 3) // old 2DS shares a single backlight across both screens
    {
        gspLcdInit();
        enable ? GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM) : GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
        gspLcdExit();
    }
}

aptHookCookie(cookie);

void checkAptHook(APT_HookType hook, void UNUSED *param) {
    if (!menu_mode) {
        switch(hook) {
            case APTHOOK_ONSUSPEND : set_bottom_screen(1);
                break;
            case APTHOOK_ONRESTORE :
            case APTHOOK_ONWAKEUP  : set_bottom_screen(0);
                break;
            default:
                break;
        }
    }
}

static void gfx_3ds_init(UNUSED const char *game_name, UNUSED bool start_in_fullscreen)
{
    if (checkN3DS())
        osSetSpeedupEnable(true);

    gfxInitDefault();
    consoleSelect(consoleInit(GFX_BOTTOM, &gConsole));

    Result rc = cfguInit();
    if (R_SUCCEEDED(rc))
    {
        u8 model;
        rc = CFGU_GetSystemModel(&model);
        if (R_SUCCEEDED(rc))
            n3ds_model = model;
        cfguExit();
    }

    init_top_screens();
    set_bottom_screen(0);
    aptHook(&cookie, checkAptHook, NULL);
}

static void gfx_set_keyboard_callbacks(UNUSED bool (*on_key_down)(int scancode), UNUSED bool (*on_key_up)(int scancode), UNUSED void (*on_all_keys_up)(void))
{
}

static void gfx_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(bool is_now_fullscreen))
{
}

static void gfx_set_fullscreen(UNUSED bool enable)
{
}

static void gfx_3ds_main_loop(void (*run_one_game_iter)(void))
{
    while (aptMainLoop())
    {
        if (!menu_mode)
        {
            run_one_game_iter();
        }
        else
        {
            int res = display_menu(&gfx_config);
            set_bottom_screen(1);
            if (res < 0)
                break;
            if (res > 0)
            {
                // exited menu so reinit screens
                stop_top_screens();
                init_top_screens();
                consoleClear();
                menu_mode = false;
                set_bottom_screen(0);
            }
        }
    }

    ndspExit();
    C3D_Fini();
    gfxExit();
}

static void gfx_3ds_get_dimensions(uint32_t *width, uint32_t *height)
{
    *width = 400;
    *height = 240;
}

static void gfx_3ds_handle_events(void)
{
    // as good a time as any
    gSliderLevel = osGet3DSliderState();

    // check if screen is pressed
    hidScanInput();
    u32 kDown = keysHeld();
    if (kDown & KEY_TOUCH)
        menu_mode = true;
}

static bool gfx_3ds_start_frame(void)
{
    return true;
}

static void gfx_3ds_swap_buffers_begin(void)
{
}

static void gfx_3ds_swap_buffers_end(void)
{
}

static double gfx_3ds_get_time(void)
{
    return 0.0;
}

struct GfxWindowManagerAPI gfx_3ds =
{
    gfx_3ds_init,
    gfx_set_keyboard_callbacks,
    gfx_set_fullscreen_changed_callback,
    gfx_set_fullscreen,
    gfx_3ds_main_loop,
    gfx_3ds_get_dimensions,
    gfx_3ds_handle_events,
    gfx_3ds_start_frame,
    gfx_3ds_swap_buffers_begin,
    gfx_3ds_swap_buffers_end,
    gfx_3ds_get_time
};

#endif
