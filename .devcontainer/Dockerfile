# syntax=docker/dockerfile:1.7
FROM node:22-bookworm@sha256:8a34c4ab3ea2c5cd194f07e317b2a8f09461d3c8b05c4e34c8ccd56d56024c4d

ARG DEV_UID=1001
ARG DEV_GID=1001
ARG PLATFORMIO_VERSION=6.1.19
ARG UV_VERSION=0.12.5
ARG CLAUDE_CODE_VERSION=2.1.246

ENV DEBIAN_FRONTEND=noninteractive \
    PLATFORMIO_CORE_DIR=/home/developer/.platformio \
    npm_config_cache=/home/developer/.npm \
    PATH=/home/developer/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       bash-completion ca-certificates curl git jq openssh-client \
       python3 python3-venv tini udev \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid "${DEV_GID}" developer \
    && useradd --uid "${DEV_UID}" --gid "${DEV_GID}" --create-home --shell /bin/bash developer

# uv provides an isolated, pinned PlatformIO installation without touching the
# system Python. Claude Code is pinned as well so rebuilding the image does not
# silently change either agent or toolchain behavior.
RUN curl -LsSf "https://astral.sh/uv/${UV_VERSION}/install.sh" -o /tmp/install-uv.sh \
    && sh /tmp/install-uv.sh \
    && install -m 0755 /root/.local/bin/uv /usr/local/bin/uv \
    && install -m 0755 /root/.local/bin/uvx /usr/local/bin/uvx \
    && rm -rf /root/.local /tmp/install-uv.sh \
    && npm install -g --allow-scripts=@anthropic-ai/claude-code "@anthropic-ai/claude-code@${CLAUDE_CODE_VERSION}" \
    && mkdir -p /home/developer/.platformio /home/developer/.npm /home/developer/.claude /workspace \
    && chown -R developer:developer /home/developer /workspace

USER developer
WORKDIR /workspace

RUN uv tool install "platformio==${PLATFORMIO_VERSION}" \
    && git config --global --add safe.directory /workspace

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["sleep", "infinity"]
