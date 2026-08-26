#!/usr/bin/env bash
set -euo pipefail

root="$(git rev-parse --show-toplevel)"
compose=(docker compose -f "$root/.devcontainer/compose.yaml" --project-directory "$root/.devcontainer")

usage() {
  cat <<'EOF'
Usage: scripts/dev-container.sh COMMAND [ARGS...]

Commands:
  up              Build and start the persistent development container
  down            Stop the container (named caches are preserved)
  shell           Open a shell in the running container
  build           Build web UI, firmware, and LittleFS image
  claude [ARGS]   Run Claude Code interactively in the container
  exec CMD...     Run an arbitrary command in the container
  status          Show container status
EOF
}

ensure_up() {
  "${compose[@]}" up -d --build
}

case "${1:-}" in
  up)
    ensure_up
    ;;
  down)
    "${compose[@]}" down
    ;;
  shell)
    ensure_up
    exec "${compose[@]}" exec dev bash
    ;;
  build)
    ensure_up
    "${compose[@]}" exec -T dev bash -lc 'npm ci && npm run build && pio run && pio run -t buildfs'
    ;;
  claude)
    shift
    ensure_up
    exec "${compose[@]}" exec dev claude "$@"
    ;;
  exec)
    shift
    if (( $# == 0 )); then usage; exit 2; fi
    ensure_up
    exec "${compose[@]}" exec dev "$@"
    ;;
  status)
    "${compose[@]}" ps
    ;;
  *)
    usage
    exit 2
    ;;
esac
