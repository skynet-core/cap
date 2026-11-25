#!/usr/bin/env bash
set -euo pipefail
: "${CAP_MODEL:?Set CAP_MODEL to the verified local checkpoint path or model ID}"
CAP_TP="${CAP_TP:-1}"
CAP_GPU_UTIL="${CAP_GPU_UTIL:-0.85}"
CAP_HOST="${CAP_HOST:-10.20.0.1}"
CAP_PORT="${CAP_PORT:-11535}"
exec vllm serve "$CAP_MODEL" \
	--served-model-name nvidia/Qwen3.8-27B-NVFP4 \
	--host "$CAP_HOST" --port "$CAP_PORT" \
	--tensor-parallel-size "$CAP_TP" \
	--max-model-len 32768 --max-num-seqs 4 \
	--gpu-memory-utilization "$CAP_GPU_UTIL" \
	--enable-prefix-caching
