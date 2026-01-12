#!/usr/bin/env python3
"""
复制p0loss头权重到p1loss头
用于对抗训练：将当前玩家策略头的权重复制到对手策略头
"""

import torch
import sys
import os

def copy_p0loss_to_p1loss(checkpoint_path, output_path):
    """
    复制p0loss头的权重到p1loss头

    Args:
        checkpoint_path: 输入checkpoint文件路径
        output_path: 输出checkpoint文件路径
    """
    print(f"Loading checkpoint from {checkpoint_path}")
    data = torch.load(checkpoint_path, map_location="cpu")

    # 检查是否有model状态
    if "model" not in data:
        print("Error: checkpoint does not contain 'model' key")
        sys.exit(1)

    model_state = data["model"]

    # 复制policy_head的conv2p权重: 通道0 -> 通道1
    if "policy_head.conv2p.weight" in model_state:
        print("Copying policy_head.conv2p weights: channel 0 -> channel 1")
        model_state["policy_head.conv2p.weight"][1] = model_state["policy_head.conv2p.weight"][0].clone()
    else:
        print("Warning: policy_head.conv2p.weight not found")

    # 复制policy_head的linear_pass权重: 通道0 -> 通道1
    if "policy_head.linear_pass.weight" in model_state:
        print("Copying policy_head.linear_pass weights: channel 0 -> channel 1")
        model_state["policy_head.linear_pass.weight"][1] = model_state["policy_head.linear_pass.weight"][0].clone()
    else:
        print("Warning: policy_head.linear_pass.weight not found")

    # 如果有linear_pass2，也复制
    if "policy_head.linear_pass2.weight" in model_state:
        print("Copying policy_head.linear_pass2 weights: channel 0 -> channel 1")
        model_state["policy_head.linear_pass2.weight"][1] = model_state["policy_head.linear_pass2.weight"][0].clone()
    else:
        print("Warning: policy_head.linear_pass2.weight not found")

    # 如果有intermediate_policy_head，也复制
    if "intermediate_policy_head.conv2p.weight" in model_state:
        print("Copying intermediate_policy_head.conv2p weights: channel 0 -> channel 1")
        model_state["intermediate_policy_head.conv2p.weight"][1] = model_state["intermediate_policy_head.conv2p.weight"][0].clone()
    else:
        print("Note: intermediate_policy_head.conv2p.weight not found (optional)")

    if "intermediate_policy_head.linear_pass.weight" in model_state:
        print("Copying intermediate_policy_head.linear_pass weights: channel 0 -> channel 1")
        model_state["intermediate_policy_head.linear_pass.weight"][1] = model_state["intermediate_policy_head.linear_pass.weight"][0].clone()

    if "intermediate_policy_head.linear_pass2.weight" in model_state:
        print("Copying intermediate_policy_head.linear_pass2 weights: channel 0 -> channel 1")
        model_state["intermediate_policy_head.linear_pass2.weight"][1] = model_state["intermediate_policy_head.linear_pass2.weight"][0].clone()

    # 保存修改后的checkpoint
    print(f"Saving modified checkpoint to {output_path}")
    torch.save(data, output_path)
    print("Done!")

    # 验证复制结果
    print("\nVerification:")
    if "policy_head.conv2p.weight" in model_state:
        diff = torch.abs(model_state["policy_head.conv2p.weight"][0] - model_state["policy_head.conv2p.weight"][1]).max()
        print(f"  policy_head.conv2p weight diff: {diff.item():.10f}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python copy_p0loss_to_p1loss.py <input_checkpoint> <output_checkpoint>")
        sys.exit(1)

    copy_p0loss_to_p1loss(sys.argv[1], sys.argv[2])