#  Flappy Bird AI using Imitation Learning (DAgger)

An autonomous AI agent trained to play **Flappy Bird** from scratch using **Imitation Learning** via the **DAgger (Dataset Aggregation)** algorithm in PyTorch.

---

## Demo



https://github.com/user-attachments/assets/aec0d2c8-4076-4226-b910-b1dc8a91976d


<!-- You can replace this demo GIF or video with your recorded screen capture! -->
<!-- Tip: GitHub supports drag-and-drop video/GIF uploads directly into README issues or Markdown! -->

> *The trained neural network agent autonomously navigating pipes in real-time.*

---

## What is Imitation Learning & DAgger?

In standard **Behavioral Cloning (Supervised Learning)**, an AI simply tries to memorize recorded actions from an expert. However, when the AI makes a small mistake during testing, it enters unseen states ("covariate shift"), panics, and crashes.

**DAgger (Dataset Aggregation)** solves this problem iteratively:
1. **Explore:** The agent plays the game and makes decisions.
2. **Consult Expert:** At every step (even in novel or awkward states the AI stumbled into), the expert is asked: *"What would you have done here?"*
3. **Aggregate:** The expert's corrections are appended to the dataset.
4. **Retrain:** The neural network is retrained on the expanded dataset, making it progressively more robust.

---

## Architecture & Methodology

- **Game Environment:** [PyGame Learning Environment (PLE)](https://github.com/ntasfi/PyGame-Learning-Environment) - Flappy Bird.
- **State Representation (8 Features):**
  - Player Y position & velocity
  - Distance to next pipe, top pipe Y, bottom pipe Y
  - Distance to next-next pipe, top pipe Y, bottom pipe Y
- **Neural Network Model:**
  - Multi-Layer Perceptron (MLP) built with **PyTorch**:
  - `Linear(8, 64) -> ReLU -> Linear(64, 64) -> ReLU -> Linear(64, 2)`
  - Outputs logits for actions: `0` (Do nothing) or `1` (Flap).
- **Class Imbalance Handling:**
  - In Flappy Bird, the agent must *not* flap 90%+ of the time. We apply class-weighted Cross-Entropy loss so the model doesn't degenerate into never jumping.

---

##  Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/Siddharth-33/Flappy-Bird-Using-Immitation-Learning-
cd flappy-bird-dagger
```

### 2. Set Up Environment
It is recommended to use Python 3.10+ in a virtual environment:

```bash
python3 -m venv env
source env/bin/activate
```

Install requirements:
```bash
pip install -r requirements.txt
```

*(Note: PyGame-Learning-Environment will be installed directly from source via `requirements.txt`)*

### 3. Run Training & Testing
```bash
python flappy_dagger.py
```
- **Phase 1 (Training):** Headless and fast. Runs DAgger iterations, collecting states and training the PyTorch model.
- **Phase 2 (Evaluation):** Automatically launches a PyGame visual window showing the trained agent playing live!

---

## Results

| Metric | Value |
|---|---|
| Initial Iteration Score | 0 - 1 |
| Trained AI Score | 50+ pipes |
| Training Time | ~50cseconds (CPU) |

