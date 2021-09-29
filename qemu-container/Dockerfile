FROM ubuntu:focal

RUN echo 'set auto-load safe-path /' > /root/.gdbinit
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y apt-utils git build-essential gdb-multiarch \
        qemu-system-misc=1:4.2-3ubuntu6 gcc-riscv64-linux-gnu \
        binutils-riscv64-linux-gnu emacs-nox \
        locales screen tmux vim wget unzip \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen es_ES.UTF-8
ENV LANG es_ES.UTF-8
ENV LC_ALL es_ES.UTF-8

VOLUME ["/root/deiso"]
WORKDIR "/root"
ENTRYPOINT "/bin/bash"
