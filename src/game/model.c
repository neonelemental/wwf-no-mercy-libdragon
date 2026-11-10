#include "model.h"

#include "gu.h"

// An angle of the animation data, 1024 to a turn, in degrees.
static const double ANGLE = 360.0 / 1024;

// The last place that a pose gave whole. A short place is a step from it.
extern float D_80069F10[3];

// The joints of a pose, in the game's data: where each is at rest in x and
// y, the order to make them in, the parent of each or 0 for a joint with a
// place of its own, and 0 or 1 for a hand or -1.
extern s8 D_80047ED0[POSE_JOINTS][2];
extern s8 D_80047EF8[POSE_JOINTS];
extern s8 D_80047F0C[POSE_JOINTS];
extern s8 D_80047F40[POSE_JOINTS];
// The kind of hands of each slot, and for each kind their scale and where
// their pivot is.
extern s8 D_8006A268[];
extern float D_80047F54[], D_80047F68[], D_80047F7C[];
extern float *D_80047FE0[2];
extern float D_80047FB8[], D_80047FCC[];
// The share of the way to the next pose that a step moves.
extern float D_8004800C[3];

// The slot before D_8009FDE0 stands D_800A9352 higher.
extern u16 D_8009FDE0, D_800A9352;
// The flags of each wrestler's model, and where each looks.
extern s32 D_801593D8_ovl_e[MODEL_SLOTS];
extern float D_801593A8_ovl_e[MODEL_SLOTS][3];

// Routines of the game that are still assembly.
void func_80010FC4(Matrix m, float x, float y, float z);        // sets the place of a matrix
float func_80011270(float degrees);                              // brings an angle into -180 to 180
float func_80011324(float from, float to);                       // the way from one angle to another
float func_8000567C(float x);                                    // its size
void func_80036140(Matrix m, float x, float y, float z);        // a rotation by three angles
void func_80002268(ModelNode *node);                             // the node walk
s32 func_80011A54(s32 file);                                     // the frames of a file
void func_80017160(s32 slot, s32 value);
void func_80012A8C(s32 slot, s32 file);                          // starts the read of a file
void func_80012B24(s32 slot);                                    // takes the next pose from the cartridge
void func_80013EB0(s32 slot);                                    // stops the sounds of a slot
void func_80013234(s32 slot);                                    // puts the pose in the joints as it is
void func_80012F48(s32 slot, s32 frame, s32 flags);              // eases from the last file
void func_80013860(s32 slot, s32 frame);                         // the events of a frame
s32 func_8012FBD0(s32 fighter, float *look);                     // where a wrestler looks
void func_8012F980(Fighter *fighter, s32 file, s32 flags);       // starts a file

static u32 word(const u8 *p)
{
    return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

// A sign bit above the bits of a size.
static int signed_size(u32 v, u32 sign)
{
    return v & sign ? -(int)(v & (sign - 1)) : (int)(v & (sign - 1));
}

u8 *pose_read(u8 *data, float angle[3], float place[3])
{
    u32 w = word(data);
    data += 4;
    float x = (float)((double)(float)(int)(w & 0x3FF) * ANGLE);
    float y = (float)((double)(float)(int)(w >> 10 & 0x3FF) * ANGLE);
    float z = (float)((double)(float)(int)(w >> 20 & 0x3FF) * ANGLE);
    angle[0] = x;
    angle[1] = y;
    angle[2] = z;
    if (!(w & 0x80000000)) {
        return data;
    }
    if (w & 0x40000000) {
        // Three bytes, each a step in halves.
        int sx = signed_size(data[0], 0x80);
        int sy = signed_size(data[1], 0x80);
        int sz = signed_size(data[2], 0x80);
        data += 3;
        place[0] = (float)((double)(float)sx * 0.5) + D_80069F10[0];
        place[1] = (float)((double)(float)sy * 0.5) + D_80069F10[1];
        place[2] = (float)((double)(float)sz * 0.5) + D_80069F10[2];
    } else {
        // A word of 11, 11 and 10 bits.
        u32 p = word(data);
        data += 4;
        place[0] = (float)signed_size(p, 0x400);
        place[1] = (float)signed_size(p >> 11, 0x400);
        place[2] = (float)signed_size(p >> 22, 0x200);
        D_80069F10[0] = place[0];
        D_80069F10[1] = place[1];
        D_80069F10[2] = place[2];
    }
    return data;
}

void anim_read(s32 slot_word, s32 step_word)
{
    s16 slot = slot_word;
    int step = (s16)step_word % 3;
    float place[POSE_JOINTS][3];
    float angle[POSE_JOINTS][3];

    func_80012B24(slot);
    Animation *anim = &D_80069F20[slot];
    u8 *data = anim->data[anim->now];
    for (int n = 1; n < POSE_JOINTS; n++) {
        data = pose_read(data, angle[n], place[n]);
    }
    float *share = &D_8004800C[step];
    for (int n = 1; n < POSE_JOINTS; n++) {
        int j = D_80047EF8[n];
        Joint *joint = &D_800A1130[slot][j];
        float *turn = angle[j];
        if (D_80047F0C[j] == 0) {
            joint->place[0] = joint->place[0] + (place[j][0] - joint->place[0]) * *share;
            joint->place[1] = joint->place[1] + (place[j][1] - joint->place[1]) * *share;
            joint->place[2] = joint->place[2] + (place[j][2] - joint->place[2]) * *share;
        }
        turn[0] = func_80011324(joint->angle[0], turn[0]);
        turn[1] = func_80011324(joint->angle[1], turn[1]);
        turn[2] = func_80011324(joint->angle[2], turn[2]);
        // Past 120 about x or z, both go by 180 less.
        if (120.0 < (double)func_8000567C(turn[0]) || 120.0 < (double)func_8000567C(turn[2])) {
            turn[0] = func_80011270(turn[0] + 180.0f);
            turn[2] = func_80011270(turn[2] + 180.0f);
        }
        joint->angle[0] = joint->angle[0] + turn[0] * *share;
        joint->angle[1] = joint->angle[1] + turn[1] * *share;
        joint->angle[2] = joint->angle[2] + turn[2] * *share;
    }
}

void joint_chain(s32 slot_word)
{
    s16 slot = slot_word;
    Matrix bone[POSE_JOINTS];
    Matrix pivot;

    for (int n = 1; n < POSE_JOINTS; n++) {
        int j = D_80047EF8[n];
        ModelNode *node = &D_80091CC0[slot][j];
        Joint *joint = &D_800A1130[slot][j];
        int parent = D_80047F0C[j];
        if (parent != 0) {
            guMtxXFMF(bone[parent],
                (float)(D_80047ED0[j][0] - D_80047ED0[parent][0]),
                (float)(D_80047ED0[j][1] - D_80047ED0[parent][1]),
                0.0f, &joint->place[0], &joint->place[1], &joint->place[2]);
        }
        mtx_rotate(bone[j], joint->angle[0], joint->angle[1], joint->angle[2]);
        func_80010FC4(bone[j], joint->place[0], joint->place[1], joint->place[2]);
        if (D_80047F40[j] == -1) {
            guTranslateF(pivot, (float)-D_80047ED0[j][0], (float)-D_80047ED0[j][1], 0.0f);
        } else {
            int kind = D_8006A268[slot];
            guScaleF(pivot, D_80047F54[kind], D_80047F68[kind], D_80047F7C[kind]);
            int hand = D_80047F40[j];
            kind = D_8006A268[slot];
            float y = D_80047FB8[kind] - (float)D_80047ED0[j][1];
            float x = D_80047FE0[hand][kind] - (float)D_80047ED0[j][0];
            func_80010FC4(pivot, x, y, D_80047FCC[kind]);
        }
        mtx_mul(pivot, bone[j], node->local);
        float px = joint->place[0], py = joint->place[1], pz = joint->place[2];
        float ax = joint->angle[0], ay = joint->angle[1], az = joint->angle[2];
        joint->last_place[0] = px;
        joint->last_place[1] = py;
        joint->last_place[2] = pz;
        joint->last_angle[0] = ax;
        joint->last_angle[1] = ay;
        joint->last_angle[2] = az;
    }
}

// An angle of 4096 to a turn, in whole degrees.
static s32 degrees(u32 angle)
{
    return (s32)(angle * 360) / 4096 % 4096;
}

void model_update(s32 slot_word, s32 file_word, s32 frame_word, ModelPlace *at, float *look, s32 flags_word, s32 lean_x, s32 lean_z)
{
    s16 slot = slot_word;
    s16 file = file_word;
    u16 frame = frame_word;
    u8 flags = flags_word;
    Animation *anim = &D_80069F20[slot];
    Joint *joints = D_800A1130[slot];
    ModelNode *nodes = D_80091CC0[slot];
    Matrix root, about_y, leaning;
    float kept[3];
    int moved = 0, posed = 0;

    s16 lift = 0;
    if (slot == D_8009FDE0 - 1) {
        lift = D_800A9352;
    }
    float about_x = 0.0f, about_z = 0.0f;
    if (lean_x != 0) {
        about_x = (float)degrees(0x1000 - (u32)lean_x);
    }
    if (lean_z != 0) {
        about_z = (float)degrees((u32)lean_z + 0x1000);
    }
    float turn = -(float)((double)(s32)((u32)at->turn + 0x800) * (360.0 / 4096));

    if (file != anim->file) {
        anim->frame = -1;
        frame = 0;
        func_80017160(slot, 0);
    }
    if (frame == 0 && anim->frame != 0) {
        anim->same_file = file == anim->file;
        func_80012A8C(slot, file);
        func_80013EB0(slot);
        u8 fresh = anim->fresh;
        anim->file = file;
        if (!fresh) {
            // Where the wrestler was, to stay there under the new file.
            kept[0] = joints[1].world[0];
            kept[1] = joints[1].world[1];
            kept[2] = joints[1].world[2];
            moved = 1;
        }
    }
    if ((s16)frame != anim->frame) {
        anim->frame = frame;
        if ((s16)frame < (s16)func_80011A54(file)) {
            // An even frame is a pose of the file, and so is its last one. An
            // odd frame is half the way to the next pose.
            if ((frame & 1) && (s16)frame != (s16)func_80011A54(file) - 1) {
                anim_read(slot, 0);
            } else {
                func_80013234(slot);
                if ((s16)frame == (s16)func_80011A54(file) - 2) {
                    func_80012B24(slot);
                }
            }
            if (flags & 2) {
                if ((anim->fresh == 0) & ((s16)frame < 5) && anim->same_file == 0) {
                    func_80012F48(slot, (s16)frame, (s8)flags);
                }
            }
            if (look) {
                joints[5].angle[0] = (float)((double)look[0] + 180.0);
                float y = look[1];
                joints[5].angle[2] = 180.0f;
                joints[5].angle[1] = y;
            }
            joint_chain(slot);
            posed = 1;
        }
    }

    if ((flags & 1) && moved) {
        // The first joint under the new file, and the wrestler back by as far as it moved.
        ModelNode *node = &nodes[1];
        Joint *joint = &joints[1];
        mtx_rotate_y(about_y, turn);
        func_80036140(leaning, about_x, 0.0f, about_z);
        mtx_mul(leaning, about_y, root);
        func_80010FC4(root, joints[0].place[0], joints[0].place[1], joints[0].place[2]);
        mtx_mul(anim->base, root, node->parent->world);
        mtx_mul(node->local, node->parent->world, node->world);
        guMtxXFMF(node->world, joint->point[0], joint->point[1], joint->point[2],
            &joint->world[0], &joint->world[1], &joint->world[2]);
        joint->world[0] = -joint->world[0];
        at->x = at->x + (kept[0] - joint->world[0]);
        at->z = at->z + (kept[2] - joint->world[2]);
        if (flags & 8) {
            at->y = at->y + (kept[1] - joint->world[1]);
        }
    }

    joints[0].place[0] = -at->x;
    joints[0].place[1] = at->y + (float)lift;
    joints[0].place[2] = at->z;
    mtx_rotate_y(about_y, turn);
    func_80036140(leaning, about_x, 0.0f, about_z);
    mtx_mul(leaning, about_y, root);
    func_80010FC4(root, joints[0].place[0], joints[0].place[1], joints[0].place[2]);
    mtx_mul(anim->base, root, nodes[0].local);
    func_80002268(&nodes[0]);
    joints[0].heading = turn;
    // Two points of each bone, for the routines that read the bones later.
    for (int n = 1; n < MODEL_JOINTS; n++) {
        if (n == 20 || n == 21) {
            continue;
        }
        Joint *joint = &joints[n];
        ModelNode *node = &nodes[n];
        joint->heading = joint->angle[1] + turn;
        guMtxXFMF(node->world, joint->point[0], joint->point[1], joint->point[2],
            &joint->world[0], &joint->world[1], &joint->world[2]);
        joint->world[0] = -joint->world[0];
        guMtxXFMF(node->world, joint->point2[0], joint->point2[1], joint->point2[2],
            &joint->world2[0], &joint->world2[1], &joint->world2[2]);
        joint->world2[0] = -joint->world2[0];
    }
    if (posed) {
        func_80013860(slot, (s16)(frame + 1));
    }
    if (anim->fresh) {
        anim->fresh = 0;
    }
}

void fighter_model(Fighter *fighter, float *look, s32 flags)
{
    ModelPlace at = { fighter->place[0], fighter->place[1], fighter->place[2], fighter->angle[1] };
    s32 lean_x = 0, lean_z = 0;
    if (fighter->flags & 8) {
        lean_x = fighter->angle[0];
        lean_z = fighter->angle[2];
    }
    model_update(fighter->slot, fighter->file, fighter->frame, &at, look, (s8)flags, lean_x, lean_z);
    fighter->place[0] = at.x;
    fighter->place[1] = at.y;
    fighter->place[2] = at.z;
}

void model_loop(void)
{
    for (int i = 0; i < MODEL_SLOTS; i++) {
        Fighter *fighter = &D_80159990_ovl_e[i];
        if (fighter->id == -1) {
            continue;
        }
        float *look = D_801593A8_ovl_e[i];
        if (!func_8012FBD0(i, look)) {
            look = NULL;
        }
        if (fighter->left <= 0) {
            if (!fighter->loop) {
                fighter_model(fighter, look, D_801593D8_ovl_e[i]);
                continue;
            }
            func_8012F980(fighter, fighter->file, 0);
        }
        fighter_model(fighter, look, D_801593D8_ovl_e[i]);
        if (fighter->left > 0) {
            fighter->frame = fighter->frame + 1;
            fighter->left = fighter->left - 1;
        }
    }
}
