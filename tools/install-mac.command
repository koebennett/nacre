#!/bin/bash
# Installs Nacre for the current user and clears the macOS download quarantine flag (otherwise hosts refuse to load it).
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
[ -d Nacre.vst3 ] && cp -R Nacre.vst3 ~/Library/Audio/Plug-Ins/VST3/ && xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Nacre.vst3
[ -d Nacre.component ] && cp -R Nacre.component ~/Library/Audio/Plug-Ins/Components/ && xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Nacre.component
echo "Installed. In FL Studio: Options > Manage plugins > Find installed plugins."
