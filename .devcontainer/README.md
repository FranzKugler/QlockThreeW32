# QlockThreeW32 development container

The container pins the development tools used by this repository:

- Debian 12 / Node.js 22
- Python 3.11
- PlatformIO Core 6.1.19
- Claude Code 2.1.246
- uv 0.12.5

The repository is bind-mounted at `/workspace`. PlatformIO, npm, and Claude
state use separate named volumes, so package downloads and authentication
survive container recreation. The container has no Docker socket, no devices,
no added capabilities, is not privileged, and runs as UID/GID 1001.

## Commands

From the repository root:

```bash
./scripts/dev-container.sh up       # build and start
./scripts/dev-container.sh shell    # interactive shell
./scripts/dev-container.sh build    # npm + firmware + LittleFS
./scripts/dev-container.sh claude   # interactive Claude Code
./scripts/dev-container.sh down     # stop; keep caches
```

Claude authentication is intentionally separate from the host credential
store. Run `./scripts/dev-container.sh claude`, complete `claude auth login`
once, and the login is retained in the `claude-state` volume.

The Vite UI is bound to host loopback port 5173. The mock API remains on port
8080 inside the container but is published as `127.0.0.1:18080`, because port
8080 on this server is already used by another service.

## VS Code

Open the repository and choose **Dev Containers: Reopen in Container**. The
configuration is in `devcontainer.json` and installs the PlatformIO and Svelte
extensions.

## ESP32 USB access

No USB device is exposed by default. Builds and filesystem image creation work
without it. Flashing will be added as a deliberate, device-specific Compose
override after the ESP32 is physically attached and its stable `/dev/serial/by-id/`
path and group ownership are known. Do not use `privileged: true`.
