FROM --platform=linux/amd64 ubuntu:24.04

# Arguments that can be overridden in 'docker build' command:
# Versions of Android SDK and NDK.
ARG VERSION_SDK=11076708
ARG VERSION_NDK=23.1.7779620
ARG ROOT_DIR=/root

# Creating mandatory environment variables
ENV ANDROID_SDK_HOME=${ROOT_DIR}/android-sdk
ENV ANDROID_NDK_HOME=${ANDROID_SDK_HOME}/ndk/${VERSION_NDK}

# Installing basic software
RUN apt-get --allow-releaseinfo-change update && apt-get install -y --no-install-recommends \
  build-essential \
  openjdk-17-jdk-headless \
  curl \
  unzip \
  cmake \
  bash \
  nasm \
  pkg-config \
  make \
  git \
  python3-venv \
  python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /root/venv
ENV VIRTUAL_ENV /root/venv
ENV PATH /root/venv/bin:$PATH
RUN python3 -m pip install meson==1.8.0 ninja

# Install Android SDK
RUN curl https://dl.google.com/android/repository/commandlinetools-linux-${VERSION_SDK}_latest.zip --output ${ROOT_DIR}/android-sdk.zip
RUN mkdir -p ${ROOT_DIR}/android-sdk && \
  unzip -qq ${ROOT_DIR}/android-sdk.zip -d ${ROOT_DIR}/android-sdk && \
  rm ${ROOT_DIR}/android-sdk.zip
RUN ${ANDROID_SDK_HOME}/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_SDK_HOME} 'cmdline-tools;latest'
RUN yes | ${ANDROID_SDK_HOME}/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_SDK_HOME} "ndk;${VERSION_NDK}"

WORKDIR ${ROOT_DIR}/ffmpeg-android-maker
RUN git clone --depth 1 --branch v2.12 https://github.com/Javernaut/ffmpeg-android-maker.git .
COPY scripts/mediacodec.patch mediacodec.patch
RUN git apply mediacodec.patch
RUN ./ffmpeg-android-maker.sh

COPY .  ${ROOT_DIR}/rt-xr-maf
WORKDIR ${ROOT_DIR}/rt-xr-maf

RUN mkdir -p ./subprojects/FFmpeg/arm64-v8a/lib && mkdir -p ./subprojects/FFmpeg/arm64-v8a/include
RUN cp ./scripts/ffmpeg_android.meson.build ./subprojects/FFmpeg/meson.build
RUN cp -r ${ROOT_DIR}/ffmpeg-android-maker/output/lib/arm64-v8a/* ./subprojects/FFmpeg/arm64-v8a/lib
RUN cp -r ${ROOT_DIR}/ffmpeg-android-maker/output/include/arm64-v8a/* ./subprojects/FFmpeg/arm64-v8a/include

RUN meson setup --wipe -Ddebug=true ./builddir/android-aarch64 --cross-file ./crossfile/android-aarch64-docker
RUN meson compile -C ./builddir/android-aarch64

## @TODO: SWIG bindings
# WORKDIR /root/swig
# RUN sudo apt-get install build-essential libpcre2-dev libpcre3-dev autoconf automake libtool bison libboost-dev
# RUN git clone https://github.com/swig/swig.git .
# RUN ./autogen.sh && ./configure && make

CMD [ "/bin/sh", "-c", "./scripts/install_android.sh /install"]