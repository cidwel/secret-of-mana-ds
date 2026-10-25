# Build image for `python3 build.py <target> --docker`: devkitARM (3DS), JDK 17 + Android SDK/NDK (Thor), gcc + SDL2 (PC).
# The devkitPro image is linux/amd64 only; on Apple Silicon Docker runs it under emulation (slower, same result).
FROM devkitpro/devkitarm:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
        git python3 python3-pil python3-numpy openjdk-17-jdk-headless unzip curl ca-certificates make gcc g++ pkg-config libsdl2-dev zip \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_HOME=/opt/android-sdk
ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
RUN mkdir -p $ANDROID_HOME/cmdline-tools \
    && curl -fsSL -o /tmp/tools.zip https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip \
    && unzip -q /tmp/tools.zip -d $ANDROID_HOME/cmdline-tools && mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest \
    && rm /tmp/tools.zip \
    && yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses > /dev/null \
    && $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager "platforms;android-34" "build-tools;34.0.0" "ndk;28.2.13676358" "cmake;3.22.1" > /dev/null \
    && chmod -R a+rwX $ANDROID_HOME

ENV DEVKITPRO=/opt/devkitpro
# git refuses to work in a mounted checkout owned by another user without this
RUN git config --system --add safe.directory '*'
