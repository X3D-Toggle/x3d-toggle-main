#!/bin/sh
## `deploy.sh`
## Local Developer Pipeline Wrapper.
## Executes a clean teardown, rebuild, install, and triggers setup.
## Usage: sudo make deploy

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "  ❌ Error: Developer deploy.sh must be run with sudo privileges. ❌"
    exit 1
fi

export X3D_EXEC=1
_l_dir_root="$(cd "$(dirname "$0")" && pwd)"

if [ -f "$_l_dir_root/scripts/framework/framework.sh" ]; then
    . "$_l_dir_root/scripts/framework/framework.sh"
else
    echo "  ❌ Error: Could not locate framework.sh ❌"
    exit 1
fi

printf_br
printf_center "🛠️ Starting Automated X3D Developer Pipeline..."
printf_br

cd "$_l_dir_root"

printf_step "🧹 Phase 1: Cleansing environment..."
make purge </dev/null >/dev/null 2>&1

printf_step "🏗️ Phase 2: Compiling architecture (make all)..."
trap 'if [ $? -ne 0 ]; then printf_br; printf_step "❌ Compilation failed. Check syntax."; fi' EXIT
make -s all </dev/null >/dev/null 2>&1
trap - EXIT

printf_step "📦 Phase 3: FHS Deployment (make install)..."
make -s install </dev/null >/dev/null 2>&1

printf_step "⚙️ Phase 4: Launching System Configurator..."
if [ -f "./setup.sh" ]; then
    ./setup.sh
else
    printf_step "⚠️ Notice: setup.sh not found in root, skipping interactive config."
    printf_divider
fi

printf_center "✨ Developer Deployment Complete ✨"
printf_br