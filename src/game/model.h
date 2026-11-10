// The model of a wrestler: its pose from the animation data, the matrix of
// each joint, and the points of each bone in the world.

#ifndef GAME_MODEL_H
#define GAME_MODEL_H

#include "scene.h"
#include "ultra.h"

#define MODEL_SLOTS 4
// Joint 0 is the wrestler itself, and joints 1 to 19 are those of a pose.
#define POSE_JOINTS 20
#define MODEL_JOINTS 23
#define MODEL_NODES 28

// A joint of a wrestler.
typedef struct {
    float angle[3];       // in degrees, about x, y and z
    float place[3];
    float last_angle[3];  // the angles and place of the last joint chain
    float last_place[3];
    u8 unknown30[0x18];
    float point[3];       // a point of the bone, in its node
    float world[3];       // that point in the world, with x the other way
    float point2[3];
    float world2[3];
    float heading;        // its angle about y plus the turn of the wrestler
} Joint;

ULTRA_SIZE(Joint, 0x7C);
ULTRA_AT(Joint, place, 0x0C);
ULTRA_AT(Joint, last_angle, 0x18);
ULTRA_AT(Joint, last_place, 0x24);
ULTRA_AT(Joint, point, 0x48);
ULTRA_AT(Joint, world, 0x54);
ULTRA_AT(Joint, point2, 0x60);
ULTRA_AT(Joint, world2, 0x6C);
ULTRA_AT(Joint, heading, 0x78);

extern Joint D_800A1130[MODEL_SLOTS][MODEL_JOINTS];

// The nodes of each slot. Node 0 is the wrestler, and node n has the matrix
// of joint n.
extern SceneNode D_80091CC0[MODEL_SLOTS][MODEL_NODES];

// The animation that a slot plays. Its file comes from the cartridge a pose
// at a time, one pose ahead: data[now] holds the pose to read, and the other
// buffer takes the next one.
typedef struct {
    u8 fresh;             // the setup of the slot sets it, and the update clears it
    u8 same_file;         // the last start was of the file that played before it
    u8 now;
    u8 reading;           // a read into the other buffer is on its way
    s16 file;
    s16 frame;            // two frames to a pose
    u8 *data[2];
    u32 rom;              // where the next pose is on the cartridge
    s32 left;             // the bytes of the file after it
    Matrix base;          // comes in front of the matrix of the wrestler
    OSMesgQueue done[2];  // gets a message when the read into data[n] ends
    OSIoMesg read[2];
    u8 unknownB8[8];
} Animation;

ULTRA_SIZE(Animation, 0xC0);
ULTRA_AT(Animation, file, 0x04);
ULTRA_AT(Animation, frame, 0x06);
ULTRA_AT(Animation, data, 0x08);
ULTRA_AT(Animation, rom, 0x10);
ULTRA_AT(Animation, left, 0x14);
ULTRA_AT(Animation, base, 0x18);
ULTRA_AT(Animation, done, 0x58);
ULTRA_AT(Animation, read, 0x88);

extern Animation D_80069F20[MODEL_SLOTS];

// Where a wrestler stands, and its angle about y, 4096 to a turn.
typedef struct {
    float x, y, z;
    s32 turn;
} ModelPlace;

ULTRA_SIZE(ModelPlace, 0x10);

// A wrestler of a match.
typedef struct {
    u8 unknown00[0x82];
    u16 flags;            // bit 3: the model leans by angle[0] and angle[2]
    u8 unknown84[0x1A];
    s16 slot;             // of its model
    s16 id;               // -1 with no wrestler
    s16 left;             // the frames of the file still to play
    s16 frame;
    s16 file;
    u8 unknownA8[0x54];
    float place[3];
    s32 angle[3];         // about x, y and z, 4096 to a turn
    u8 unknown114[0x1D9];
    u8 loop;              // the file starts again at its end
    u8 unknown2EE[0x6A];
} Fighter;

ULTRA_SIZE(Fighter, 0x358);
ULTRA_AT(Fighter, flags, 0x82);
ULTRA_AT(Fighter, slot, 0x9E);
ULTRA_AT(Fighter, id, 0xA0);
ULTRA_AT(Fighter, left, 0xA2);
ULTRA_AT(Fighter, frame, 0xA4);
ULTRA_AT(Fighter, file, 0xA6);
ULTRA_AT(Fighter, place, 0xFC);
ULTRA_AT(Fighter, angle, 0x108);
ULTRA_AT(Fighter, loop, 0x2ED);

// The wrestlers of the match, in overlay ovl_e.
extern Fighter D_80159990_ovl_e[MODEL_SLOTS];

// Reads the pose of one joint: a word of three angles, then its place when
// the word says so. Gives the byte after the pose.
u8 *pose_read(u8 *data, float angle[3], float place[3]);

// Takes the next pose of the slot from the cartridge and moves each joint a
// share of the way to it: half for step 0.
void anim_read(s32 slot, s32 step);

// Makes the matrix of each joint of a pose from its angles, and puts it in
// the node of the joint.
void joint_chain(s32 slot);

// Gives the model of a slot its pose for FRAME of FILE, at AT. LOOK, when not
// NULL, turns the head. FLAGS: 1 keeps the wrestler in place over a change of
// file, 2 eases into a new file, 8 keeps its height too.
void model_update(s32 slot, s32 file, s32 frame, ModelPlace *at, float *look, s32 flags, s32 lean_x, s32 lean_z);

// Updates the model of one wrestler of the match, and of each.
void fighter_model(Fighter *fighter, float *look, s32 flags);
void model_loop(void);

#endif
