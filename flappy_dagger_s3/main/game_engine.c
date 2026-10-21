// ============================================================
// Flappy Bird Game Engine
// Physics matched to PLE FlappyBird for DAgger compatibility
//
// The neural network was trained in Python using PLE's coordinate
// system (288x512) with normalization: (value / 300.0) - 0.5
// This engine replicates those exact physics so the pre-trained
// weights work correctly without retraining.
// ============================================================

#include "game_engine.h"
#include <string.h>
#include <stdlib.h>
#include <esp_random.h>

// ---- Internal helpers ----

// Generate a random pipe top_y such that the gap fits on screen.
// In PLE, pipe top_y ranges roughly from 60 to (512 - 100 - 60) = 352.
static float random_pipe_top_y(void)
{
    uint32_t r = esp_random(); // Hardware RNG on ESP32
    float min_top = 60.0f;
    float max_top = (float)(GAME_HEIGHT - PIPE_GAP - 60);
    return min_top + (float)(r % (uint32_t)(max_top - min_top));
}

static void spawn_pipe(pipe_t *p, float x_pos)
{
    p->x        = x_pos;
    p->top_y    = random_pipe_top_y();
    p->bottom_y = p->top_y + PIPE_GAP;
    p->active   = true;
    p->scored   = false;
}

// ---- Public API ----

void game_init(game_state_t *gs)
{
    memset(gs, 0, sizeof(game_state_t));

    // Bird starts at 40% of screen height (matching PLE)
    gs->bird.y   = GAME_HEIGHT * 0.4f;
    gs->bird.vel = 0.0f;

    gs->score       = 0;
    gs->game_over   = false;
    gs->frame_count = 0;

    // Spawn initial pipes staggered across the screen
    for (int i = 0; i < MAX_PIPES; i++) {
        spawn_pipe(&gs->pipes[i], GAME_WIDTH + i * PIPE_SPAWN_DIST);
    }
}

void game_update(game_state_t *gs, bool flap)
{
    if (gs->game_over) return;

    gs->frame_count++;

    // ---- Bird physics (exact PLE match) ----
    if (flap) {
        gs->bird.vel = FLAP_POWER; // PLE sets velocity directly on flap
    }
    gs->bird.vel += GRAVITY;       // Gravity pulls bird down each frame
    if (gs->bird.vel > MAX_DROP_SPEED) {
        gs->bird.vel = MAX_DROP_SPEED; // Terminal velocity cap
    }
    gs->bird.y += gs->bird.vel;

    // ---- Ceiling / floor collision ----
    if (gs->bird.y < 0) {
        gs->bird.y   = 0;
        gs->bird.vel = 0;
    }
    if (gs->bird.y + BIRD_HEIGHT > GAME_HEIGHT) {
        gs->bird.y    = GAME_HEIGHT - BIRD_HEIGHT;
        gs->game_over = true;
        return;
    }

    // ---- Pipe movement & collision ----
    for (int i = 0; i < MAX_PIPES; i++) {
        pipe_t *p = &gs->pipes[i];
        if (!p->active) continue;

        p->x -= PIPE_SPEED; // Scroll pipe leftward

        // Score: bird's right edge passed pipe's right edge
        if (!p->scored && (BIRD_X + BIRD_WIDTH > p->x + PIPE_WIDTH)) {
            p->scored = true;
            gs->score++;
        }

        // Recycle pipe once it goes off-screen left
        if (p->x + PIPE_WIDTH < 0) {
            // Find the rightmost pipe to position new one after it
            float max_x = 0;
            for (int j = 0; j < MAX_PIPES; j++) {
                if (gs->pipes[j].x > max_x) {
                    max_x = gs->pipes[j].x;
                }
            }
            spawn_pipe(p, max_x + PIPE_SPAWN_DIST);
        }

        // ---- Collision detection ----
        // Check if bird overlaps this pipe horizontally
        float bird_left   = BIRD_X;
        float bird_right  = BIRD_X + BIRD_WIDTH;
        float bird_top    = gs->bird.y;
        float bird_bottom = gs->bird.y + BIRD_HEIGHT;

        float pipe_left   = p->x;
        float pipe_right  = p->x + PIPE_WIDTH;

        bool h_overlap = (bird_right > pipe_left) && (bird_left < pipe_right);

        if (h_overlap) {
            // Hit top pipe (bird is above the gap)
            if (bird_top < p->top_y) {
                gs->game_over = true;
                return;
            }
            // Hit bottom pipe (bird is below the gap)
            if (bird_bottom > p->bottom_y) {
                gs->game_over = true;
                return;
            }
        }
    }
}

// Find the two closest active pipes ahead of the bird.
// idx1 = nearest, idx2 = second nearest.
static void find_next_pipes(const game_state_t *gs, int *idx1, int *idx2)
{
    *idx1 = -1;
    *idx2 = -1;
    float best1 = 1e9f;
    float best2 = 1e9f;

    for (int i = 0; i < MAX_PIPES; i++) {
        if (!gs->pipes[i].active) continue;
        // Pipe is "ahead" if its right edge hasn't fully passed the bird
        float dist = gs->pipes[i].x + PIPE_WIDTH - BIRD_X;
        if (dist < 0) continue; // Already behind the bird

        if (dist < best1) {
            // Push current best to second
            best2 = best1;
            *idx2 = *idx1;
            best1 = dist;
            *idx1 = i;
        } else if (dist < best2) {
            best2 = dist;
            *idx2 = i;
        }
    }

    // Fallback: if no second pipe found, duplicate first
    if (*idx1 >= 0 && *idx2 < 0) {
        *idx2 = *idx1;
    }
}

void game_get_nn_state(const game_state_t *gs, float out[8])
{
    int p1, p2;
    find_next_pipes(gs, &p1, &p2);

    float player_y   = gs->bird.y;
    float player_vel  = gs->bird.vel;
    float pipe1_dist  = (p1 >= 0) ? (gs->pipes[p1].x - BIRD_X) : 300.0f;
    float pipe1_top   = (p1 >= 0) ? gs->pipes[p1].top_y         : 200.0f;
    float pipe1_bot   = (p1 >= 0) ? gs->pipes[p1].bottom_y      : 300.0f;
    float pipe2_dist  = (p2 >= 0) ? (gs->pipes[p2].x - BIRD_X) : 300.0f;
    float pipe2_top   = (p2 >= 0) ? gs->pipes[p2].top_y         : 200.0f;
    float pipe2_bot   = (p2 >= 0) ? gs->pipes[p2].bottom_y      : 300.0f;

    // Normalize exactly as Python: (value / 300.0) - 0.5
    // This MUST match extract_state() in flappy_dagger.py
    out[0] = (player_y   / 300.0f) - 0.5f;
    out[1] = (player_vel  / 300.0f) - 0.5f;
    out[2] = (pipe1_dist  / 300.0f) - 0.5f;
    out[3] = (pipe1_top   / 300.0f) - 0.5f;
    out[4] = (pipe1_bot   / 300.0f) - 0.5f;
    out[5] = (pipe2_dist  / 300.0f) - 0.5f;
    out[6] = (pipe2_top   / 300.0f) - 0.5f;
    out[7] = (pipe2_bot   / 300.0f) - 0.5f;
}

int game_to_screen_x(float gx)
{
    return (int)(gx * (float)SCREEN_WIDTH / (float)GAME_WIDTH);
}

int game_to_screen_y(float gy)
{
    return (int)(gy * (float)SCREEN_HEIGHT / (float)GAME_HEIGHT);
}
