// ============================================================
// Flappy Bird Game Engine
// Physics matched to PLE FlappyBird for DAgger compatibility
// ============================================================
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ---- Game World Constants (match PLE exactly) ----
#define GAME_WIDTH     288
#define GAME_HEIGHT    512
#define BIRD_X         80      // Bird's fixed horizontal position
#define BIRD_WIDTH     24
#define BIRD_HEIGHT    24
#define PIPE_WIDTH     52
#define PIPE_GAP       100     // Vertical gap between top and bottom pipe
#define PIPE_SPEED     4       // Pixels per frame (leftward)
#define GRAVITY        1.0f    // Downward acceleration per frame
#define FLAP_POWER     (-9.0f) // Upward velocity on flap
#define MAX_DROP_SPEED 10.0f   // Terminal falling velocity
#define MAX_PIPES      3       // Number of pipe pairs tracked
#define PIPE_SPAWN_DIST 160    // Horizontal spacing between pipe pairs

// ---- Display Scaling (2.8" ILI9341 in landscape) ----
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240

typedef struct {
    float y;           // Bird's Y position in game coordinates
    float vel;         // Bird's vertical velocity
} bird_t;

typedef struct {
    float x;           // Left edge X position in game coordinates
    float top_y;       // Bottom edge of the TOP pipe (= top of the gap)
    float bottom_y;    // Top edge of the BOTTOM pipe (= bottom of the gap)
    bool  active;      // Is this pipe pair in play?
    bool  scored;      // Has the bird passed this pipe?
} pipe_t;

typedef struct {
    bird_t  bird;
    pipe_t  pipes[MAX_PIPES];
    int     score;
    bool    game_over;
    int     frame_count;
} game_state_t;

/**
 * @brief Initialize / reset the game to starting state.
 */
void game_init(game_state_t *gs);

/**
 * @brief Advance the game by one frame.
 * @param gs      Game state
 * @param flap    true if the bird should flap this frame
 */
void game_update(game_state_t *gs, bool flap);

/**
 * @brief Extract the 8-float normalized state vector for the neural network.
 *        Normalization matches Python: (value / 300.0) - 0.5
 * @param gs      Game state
 * @param out     Output array of 8 floats
 */
void game_get_nn_state(const game_state_t *gs, float out[8]);

/**
 * @brief Convert game-world X coordinate to screen pixel.
 */
int game_to_screen_x(float gx);

/**
 * @brief Convert game-world Y coordinate to screen pixel.
 */
int game_to_screen_y(float gy);
