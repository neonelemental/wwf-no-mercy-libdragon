#include "tick.h"

#include "model.h"
#include "rope.h"
#include "scene.h"
#include "slot.h"

// What the graphics thread draws a frame from: one for each of the two
// frames that draw in turn, with which one is next and the count of ticks.
typedef struct {
    u8 unknown[8];
} Frame;

extern Frame D_80158EB0_ovl_e[2];
extern u32 D_80144CA8_ovl_e;
extern u32 D_80144CAC_ovl_e;
// The camera: where it is and what it looks at, and its roll in degrees.
extern float D_80158FF4_ovl_e[3];
extern float D_80159000_ovl_e[3];
extern float D_8015901C_ovl_e;
// The light of the match: its place, its gain and its base.
extern float D_8015900C_ovl_e[3];
extern u8 D_80144CA6_ovl_e;
extern u8 D_80144CA7_ovl_e;
// The light pass shades one slot a tick: each takes its turn in this many ticks.
extern s16 D_80158F10_ovl_e;
// For each slot, less than 0 when the match leaves it out.
extern s16 D_8016B920_ovl_e[MODEL_SLOTS];
extern u8 D_800A98C0;
extern u16 D_801594D0_ovl_e;

// Routines of the game that are still assembly.
s32 func_80131BA0(void);
s32 func_80131BB4(void);
void func_800E2344(s32 held);
void func_8000E4BC(Frame *frame, float roll, float x, float y, float z, float to_x, float to_y, float to_z);  // the camera
void func_800231A4(void);
float func_8011E128(s32 slot);  // the height of the floor under a wrestler
void func_80025F40(void);
void func_80018338(s32 held);
void func_800DE58C(s32 held);
void func_800DBD40(void);
void func_80016D2C(void);
void func_80015B68(void);
void func_80003050(Frame *frame);  // hands the frame to the graphics thread and waits for it

void match_tick(s32 held)
{
    D_800A98C0 = func_80131BA0();
    func_800E2344((s16)held);
    float *eye = D_80158FF4_ovl_e, *to = D_80159000_ovl_e;
    func_8000E4BC(&D_80158EB0_ovl_e[D_80144CA8_ovl_e], (float)((double)D_8015901C_ovl_e * 0.0174532925199432955),
        eye[0], eye[1], eye[2], to[0], to[1], to[2]);
    func_800231A4();
    if ((s16)held == 0) {
        arena_tick();
    }

    u32 turn = 0;
    for (int i = 0; i < MODEL_SLOTS; i++) {
        if (D_8016B920_ovl_e[i] < 0) {
            continue;
        }
        slot_passes(i);
        float floor = 0.0f;
        if (800.0f < D_800A1130[i][0].place[2]) {
            floor = func_8011E128(i);
        }
        slot_tick(i, 90.0f, floor);
        if (D_80144CAC_ovl_e % (u32)D_80158F10_ovl_e == turn) {
            scene_light(D_80091CC0[i], D_8015900C_ovl_e, D_80144CA6_ovl_e, D_80144CA7_ovl_e);
        }
        turn++;
    }

    if ((s16)held == 0) {
        if ((s8)D_800A98C0 == 0 || func_80131BB4()) {
            func_80025F40();
        }
    }
    func_80018338((s16)(held | D_801594D0_ovl_e));
    scene_project();
    scene_sort(1);
    func_800DE58C((s16)held);
    func_800DBD40();
    func_80016D2C();
    func_80015B68();
    func_80003050(&D_80158EB0_ovl_e[D_80144CA8_ovl_e]);
    D_80144CA8_ovl_e ^= 1;
    D_80144CAC_ovl_e++;
}
