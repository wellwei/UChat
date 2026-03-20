#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BIN="${BUILD_DIR}/test/chatserver_send_qps_benchmark"

PROCESSES="${PROCESSES:-4}"
SEND_REQUESTS="${SEND_REQUESTS:-10000}"
SYNC_REQUESTS="${SYNC_REQUESTS:-5000}"
CONCURRENCY="${CONCURRENCY:-64}"
RECEIVERS="${RECEIVERS:-256}"
SYNC_LIMIT="${SYNC_LIMIT:-500}"
ACK_AFTER_SYNC="${ACK_AFTER_SYNC:-0}"
LOG_DIR="${LOG_DIR:-${BUILD_DIR}/benchmark_logs}"

mkdir -p "${LOG_DIR}"
rm -f "${LOG_DIR}"/bench_*.log

if [[ ! -x "${BIN}" ]]; then
  echo "benchmark binary not found: ${BIN}" >&2
  echo "build it first with: cmake --build ${BUILD_DIR} --target chatserver_send_qps_benchmark -j4" >&2
  exit 1
fi

echo "Starting ${PROCESSES} benchmark processes"
echo "logs: ${LOG_DIR}"

pids=()
for ((i=0; i<PROCESSES; ++i)); do
  log_file="${LOG_DIR}/bench_${i}.log"
  extra_args=()
  if [[ "${ACK_AFTER_SYNC}" != "1" ]]; then
    extra_args+=(--no-ack-after-sync)
  fi
  "${BIN}" \
    --send-requests "${SEND_REQUESTS}" \
    --sync-requests "${SYNC_REQUESTS}" \
    --concurrency "${CONCURRENCY}" \
    --receivers "${RECEIVERS}" \
    --sync-limit "${SYNC_LIMIT}" \
    --uid-offset "${i}" \
    "${extra_args[@]}" \
    > "${log_file}" 2>&1 &
  pids+=("$!")
done

failed=0
for pid in "${pids[@]}"; do
  if ! wait "${pid}"; then
    failed=1
  fi
done

echo "Per-process results:"
grep -h "overall_success_qps=" "${LOG_DIR}"/bench_*.log || true
grep -h "^send " "${LOG_DIR}"/bench_*.log || true
grep -h "^sync " "${LOG_DIR}"/bench_*.log || true

if [[ "${failed}" -ne 0 ]]; then
  echo "One or more benchmark processes failed. Check logs under ${LOG_DIR}" >&2
  exit 1
fi

echo "All benchmark processes completed."
