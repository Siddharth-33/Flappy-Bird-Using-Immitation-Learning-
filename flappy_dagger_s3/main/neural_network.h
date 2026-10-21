// ============================================================
// Neural Network Forward Pass - DAgger Flappy Bird Agent
// Architecture: Linear(8,64) -> ReLU -> Linear(64,64) -> ReLU -> Linear(64,2)
// ============================================================
#pragma once

#include <stdint.h>

/**
 * @brief Initialize the neural network (currently a no-op, weights are in flash).
 */
void nn_init(void);

/**
 * @brief Run forward pass inference.
 *
 * @param input  Normalized 8-float state vector
 * @return int   Predicted action: 0 = Do Nothing, 1 = Flap
 */
int nn_predict(const float input[8]);

/**
 * @brief Get raw output logits from the last prediction.
 *
 * @param out_logits  Array of 2 floats to fill [logit_glide, logit_flap]
 */
void nn_get_logits(float out_logits[2]);
