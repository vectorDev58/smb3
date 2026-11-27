#pragma once
// Which controller each player reads. src/ui_host.c applies it to NESRecomp's input;
// tests/render_math.cpp checks it.
#include "cyc_input.h"
#include <string.h>

static inline void smb3_take_free_pad(bool *taken, int pads, int *pad) {
    for (int i = 0; *pad < 0 && i < pads; ++i)
        if (!taken[i]) { *pad = i; taken[i] = true; }
}

// pad[p] is an index into the frame's pads, or -1. ids are the pads' SDL instance ids. picked[p] is
// the pad chosen in Controls this session (-1: automatic); config.ini keeps its model as
// b->device[p]. coop_pad remembers the lone pad co-op gave player 2.
static inline void smb3_choose_pads(const CycBindings *b, const CycInputFrame *f, const int *ids,
                                    const int picked[CYC_INPUT_PLAYERS], bool coop, int *coop_pad,
                                    int pad[CYC_INPUT_PLAYERS]) {
    bool taken[CYC_INPUT_MAX_PADS] = {false};
    const int pads = f->pad_count < CYC_INPUT_MAX_PADS ? f->pad_count : CYC_INPUT_MAX_PADS;
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p) pad[p] = -1;
    // The pad chosen in Controls, then (after a restart) the first free pad of that model.
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p)
        for (int i = 0; b->source[p] && picked[p] >= 0 && pad[p] < 0 && i < pads; ++i)
            if (ids[i] == picked[p]) { pad[p] = i; taken[i] = true; }
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p)
        for (int i = 0; b->source[p] && b->device[p][0] && pad[p] < 0 && i < pads; ++i)
            if (!taken[i] && f->pad_guid[i] && !strcmp(f->pad_guid[i], b->device[p])) { pad[p] = i; taken[i] = true; }
    // Co-op with player 1 on the keyboard: player 2 takes a lone pad and keeps it, so a pad
    // plugged in later goes to player 1 and nobody switches characters.
    if (!coop) *coop_pad = -1;
    else if (b->source[0] == 1 && b->source[1] == 2 && pad[0] < 0 && pad[1] < 0) {
        int free_pads = 0;
        for (int i = 0; i < pads; ++i)
            if (!taken[i]) {
                ++free_pads;
                if (ids[i] == *coop_pad) pad[1] = i;
            }
        if (pad[1] >= 0) taken[pad[1]] = true;
        else if (free_pads == 1) {
            smb3_take_free_pad(taken, pads, &pad[1]);
            *coop_pad = ids[pad[1]];
        }
    }
    // Otherwise as NESRecomp: a keyboard player 1, then each controller player, takes the next
    // free pad. A keyboard player 2 comes last.
    if (b->source[0] == 1) smb3_take_free_pad(taken, pads, &pad[0]);
    for (int p = 0; p < CYC_INPUT_PLAYERS; ++p)
        if (b->source[p] == 2) smb3_take_free_pad(taken, pads, &pad[p]);
    for (int p = 1; p < CYC_INPUT_PLAYERS; ++p)
        if (b->source[p] == 1) smb3_take_free_pad(taken, pads, &pad[p]);
}
