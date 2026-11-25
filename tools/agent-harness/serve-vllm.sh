#!/usr/bin/env bash
set -euo pipefail
: "${AMP_MODEL:?Set AMP_MODEL to the verified local checkpoint path or model ID}"
AMP_TP="${AMP_TP:-1}"
AMP_GPU_UTIL="${AMP_GPU_UTIL:-0.85}"
AMP_HOST="${AMP_HOST:-10.20.0.1}"
AMP_PORT="${AMP_PORT:-11535}"
exec vllm serve "$AMP_MODEL" \
	--served-model-name nvidia/Qwen3.8-27B-NVFP4 \
	--host "$AMP_HOST" --port "$AMP_PORT" \
	--tensor-parallel-size "$AMP_TP" \
	--max-model-len 32768 --max-num-seqs 4 \
	--gpu-memory-utilization "$AMP_GPU_UTIL" \
	--enable-prefix-caching
