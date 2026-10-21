#!/usr/bin/env python3
"""
Export PyTorch DAgger FlappyBird model weights to a C header file.

Usage:
    python export_weights.py <path_to_pth> <output_header>
    python export_weights.py ../flappy_agent.pth main/model_weights.h
"""

import sys
import torch
import numpy as np


def export_to_c_header(pth_path: str, header_path: str):
    state_dict = torch.load(pth_path, map_location="cpu", weights_only=True)

    with open(header_path, "w") as f:
        f.write("// ============================================================\n")
        f.write("// Auto-generated DAgger Flappy Bird Model Weights\n")
        f.write("// Source: {}\n".format(pth_path))
        f.write("// DO NOT EDIT — regenerate with export_weights.py\n")
        f.write("// ============================================================\n")
        f.write("#pragma once\n\n")

        for name, param in state_dict.items():
            arr = param.detach().cpu().numpy()
            # Convert "network.0.weight" -> "network_0_weight"
            clean_name = name.replace(".", "_")

            if arr.ndim == 1:
                # Bias vector
                vals = ", ".join(f"{v:.7f}f" for v in arr.flatten())
                f.write(f"static const float {clean_name}[{arr.shape[0]}] = {{\n")
                # Write in rows of 8 for readability
                flat = arr.flatten()
                for i in range(0, len(flat), 8):
                    chunk = flat[i:i+8]
                    line = ", ".join(f"{v:.7f}f" for v in chunk)
                    f.write(f"    {line},\n")
                f.write("};\n\n")

            elif arr.ndim == 2:
                # Weight matrix: shape [out_features, in_features]
                rows, cols = arr.shape
                f.write(f"static const float {clean_name}[{rows}][{cols}] = {{\n")
                for r in range(rows):
                    vals = ", ".join(f"{v:.7f}f" for v in arr[r])
                    f.write(f"    {{ {vals} }},\n")
                f.write("};\n\n")

        # Also write layer dimensions for self-documentation
        f.write("// Layer dimensions summary:\n")
        for name, param in state_dict.items():
            f.write(f"//   {name}: {list(param.shape)}\n")

    total_params = sum(p.numel() for p in state_dict.values())
    print(f"✅ Exported {total_params} parameters to {header_path}")
    print(f"   Layers:")
    for name, param in state_dict.items():
        print(f"     {name}: {list(param.shape)}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        pth_path = "../flappy_agent.pth"
        header_path = "main/model_weights.h"
    else:
        pth_path = sys.argv[1]
        header_path = sys.argv[2]

    export_to_c_header(pth_path, header_path)
