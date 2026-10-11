import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from ple.games.flappybird import FlappyBird
from ple import PLE


class FlappyAgent(nn.Module):
    def __init__(self):
        super(FlappyAgent, self).__init__()
        self.network = nn.Sequential(
            nn.Linear(8, 64),
            nn.ReLU(),
            nn.Linear(64, 64),
            nn.ReLU(),
            nn.Linear(64, 2)
        )

    def forward(self, x):
        return self.network(x)


def expert_policy(state_dict):
    player_y = state_dict['player_y']
    next_pipe_top_y = state_dict['next_pipe_top_y']
    next_pipe_bottom_y = state_dict['next_pipe_bottom_y']
    target_y = (next_pipe_top_y + next_pipe_bottom_y) / 2
    if player_y > target_y + 10:
        return 1
    return 0


def extract_state(game):
    state_dict = game.getGameState()
    state = [
        state_dict['player_y'], state_dict['player_vel'],
        state_dict['next_pipe_dist_to_player'], state_dict['next_pipe_top_y'],
        state_dict['next_pipe_bottom_y'], state_dict['next_next_pipe_dist_to_player'],
        state_dict['next_next_pipe_top_y'], state_dict['next_next_pipe_bottom_y']
    ]
    return (np.array(state, dtype=np.float32) / 300.0) - 0.5, state_dict


def no_op(env):
    for _ in range(5):
        env.act(119 if np.random.randn() < 0.5 else None)


def train_dagger():
    game = FlappyBird()
    env = PLE(game, fps=30, display_screen=False, force_fps=True)
    env.init()

    agent = FlappyAgent()
    optimizer = optim.Adam(agent.parameters(), lr=1e-3)

    # We add class weights to handle the fact that 'do nothing' happens much more than 'flap'
    weights = torch.tensor([1.0, 5.0])
    criterion = nn.CrossEntropyLoss(weight=weights)

    dataset_X = []
    dataset_Y = []

    print(" Starting DAgger Training (Improved Version)...")

    for iteration in range(1, 31):
        env.reset_game()
        no_op(env)

        steps_this_iteration = 0
        score = 0

        # Collect 1000 frames of data per iteration
        while steps_this_iteration < 1000:
            if env.game_over():
                env.reset_game()
                no_op(env)

            state_vec, state_dict = extract_state(game)
            expert_action = expert_policy(state_dict)

            # 100% expert at first, slowly handing over control to AI
            expert_probability = max(0.1, 1.0 - iteration / 10.0)

            if np.random.rand() < expert_probability:
                action = expert_action
            else:
                with torch.no_grad():
                    tensor_state = torch.tensor(state_vec).unsqueeze(0)
                    logits = agent(tensor_state)
                    action = torch.argmax(logits, dim=1).item()

            dataset_X.append(state_vec)
            dataset_Y.append(expert_action)

            reward = env.act(119 if action == 1 else None)
            if reward > 0:
                score += reward
            steps_this_iteration += 1

        # Train with Mini-batches
        X_tensor = torch.tensor(np.array(dataset_X), dtype=torch.float32)
        Y_tensor = torch.tensor(np.array(dataset_Y), dtype=torch.long)

        dataset = TensorDataset(X_tensor, Y_tensor)
        dataloader = DataLoader(dataset, batch_size=128, shuffle=True)

        print(f" Iteration {iteration}: Training on {len(dataset_X)} samples...")

        agent.train()
        for epoch in range(30):  # 30 passes over the data
            for batch_X, batch_Y in dataloader:
                optimizer.zero_grad()
                predictions = agent(batch_X)
                loss = criterion(predictions, batch_Y)
                loss.backward()
                optimizer.step()

    print("Training Complete! Saving model...")
    torch.save(agent.state_dict(), "flappy_agent.pth")
    return agent


def test_agent(agent):
    print("Testing Trained Model...")
    game = FlappyBird()
    env = PLE(game, fps=30, display_screen=True, force_fps=False)
    env.init()
    env.reset_game()
    no_op(env)

    agent.eval()
    score = 0
    while not env.game_over():
        state_vec, _ = extract_state(game)
        with torch.no_grad():
            tensor_state = torch.tensor(state_vec).unsqueeze(0)
            logits = agent(tensor_state)
            action = torch.argmax(logits, dim=1).item()

        reward = env.act(119 if action == 1 else None)
        if reward > 0:
            score += reward

    print(f"Final Score: {score}")


if __name__ == "__main__":
    trained_agent = train_dagger()
    test_agent(trained_agent)
