cat > .devcontainer/devcontainer.json << 'EOF'
{
    "name": "SystemC Dev",
    "image": "mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04",
    "features": {
        "ghcr.io/devcontainers/features/node:1": {
            "version": "lts"
        }
    },
    "postCreateCommand": "sudo apt update && sudo apt install -y libsystemc-dev gtkwave && npm install -g @anthropic-ai/claude-code"
}
EOF
git add .devcontainer/devcontainer.json
git commit -m "add claude code to devcontainer"
git push