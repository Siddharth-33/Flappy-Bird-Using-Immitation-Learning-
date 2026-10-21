// ============================================================
// Neural Network Forward Pass - DAgger Flappy Bird Agent
// Pure C implementation — no external ML runtime needed.
// Weights are stored in flash (const arrays in model_weights.h).
//
// Architecture (matches PyTorch FlappyAgent):
//   Input(8) -> Linear(64) -> ReLU -> Linear(64) -> ReLU -> Linear(2)
//
// Total: ~4,866 parameters (~19.5 KB in flash)
// Inference time on ESP32-S3 @ 240MHz: < 0.1 ms
// ============================================================

#include "neural_network.h"
#include "model_weights.h"
#include <string.h>

// Store last output logits for debug display
static float last_logits[2] = {0};

void nn_init(void)
{
    // Weights are compile-time constants stored in flash — nothing to initialize.
    // This function exists for symmetry with other driver init calls.
}

int nn_predict(const float input[8])
{
    float h1[64]; // Hidden layer 1 output
    float h2[64]; // Hidden layer 2 output
    float out[2]; // Final output logits

    // ---- Layer 1: Linear(8 -> 64) + ReLU ----
    for (int i = 0; i < 64; i++) {
        float sum = network_0_bias[i];
        for (int j = 0; j < 8; j++) {
            sum += input[j] * network_0_weight[i][j];
        }
        h1[i] = (sum > 0.0f) ? sum : 0.0f; // ReLU activation
    }

    // ---- Layer 2: Linear(64 -> 64) + ReLU ----
    for (int i = 0; i < 64; i++) {
        float sum = network_2_bias[i];
        for (int j = 0; j < 64; j++) {
            sum += h1[j] * network_2_weight[i][j];
        }
        h2[i] = (sum > 0.0f) ? sum : 0.0f; // ReLU activation
    }

    // ---- Layer 3: Linear(64 -> 2) — no activation ----
    for (int i = 0; i < 2; i++) {
        float sum = network_4_bias[i];
        for (int j = 0; j < 64; j++) {
            sum += h2[j] * network_4_weight[i][j];
        }
        out[i] = sum;
    }

    // Save logits for debug overlay
    last_logits[0] = out[0];
    last_logits[1] = out[1];

    // Argmax: 0 = Glide (do nothing), 1 = Flap
    return (out[1] > out[0]) ? 1 : 0;
}

void nn_get_logits(float out_logits[2])
{
    out_logits[0] = last_logits[0];
    out_logits[1] = last_logits[1];
}
