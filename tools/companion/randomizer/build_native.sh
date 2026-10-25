#!/bin/bash
# Builds the headless randomizer from an upstream checkout, outside the repository tree.
#   build_native.sh exe-aot      NativeAOT executable for this machine (linux-x64)
#   build_native.sh lib-aot      NativeAOT shared library with the C ABI of NativeExports.cs (linux-x64)
#   build_native.sh lib-android  NativeAOT shared library for Android arm64 (linux-bionic-arm64, NDK clang)
# Environment: SOM_RANDO_SRC (upstream SoMRandomizer/ project folder), SOM_CLI_STATE (output root).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
STATE=${SOM_CLI_STATE:-/home/cidwel/.local/state/som-decomp/companion-cli-20261017}
export SOM_RANDO_SRC=${SOM_RANDO_SRC:-$STATE/repo-moppu/SecretOfManaRandomizer/SoMRandomizer}
export NUGET_PACKAGES=${NUGET_PACKAGES:-$STATE/aot/nuget}
what=${1:?usage: build_native.sh exe-aot|lib-aot|lib-android}
common=(-c Release -p:BaseIntermediateOutputPath=$STATE/aot/obj-$what/ -p:BaseOutputPath=$STATE/aot/bin-$what/ -o "$STATE/aot/$what")
cd "$HERE/headless"
case $what in
  exe-aot)  dotnet publish "${common[@]}" -r linux-x64 -p:PublishAot=true -p:CppCompilerAndLinker=gcc -p:LinkerFlavor=bfd ;;
  lib-aot)  dotnet publish "${common[@]}" -r linux-x64 -p:SomRandoNativeLib=1 -p:PublishAot=true -p:NativeLib=Shared -p:CppCompilerAndLinker=gcc -p:LinkerFlavor=bfd ;;
  lib-android)
    NDK=${ANDROID_NDK:-$(ls -d "$HOME"/Android/Sdk/ndk/28* | head -1)}
    T=$NDK/toolchains/llvm/prebuilt/linux-x86_64
    dotnet publish "${common[@]}" -r linux-bionic-arm64 -p:SomRandoNativeLib=1 -p:PublishAot=true -p:NativeLib=Shared \
      -p:CppCompilerAndLinker=$T/bin/aarch64-linux-android24-clang -p:SysRoot=$T/sysroot -p:ObjCopyName=$T/bin/llvm-objcopy -p:LinkerFlavor=lld ;;
esac
