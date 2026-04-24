# Этап 1: сборка
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-all-dev \
    libgumbo-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Этап 2: финальный образ
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    libboost-system1.74.0 \
    libgumbo1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/bsu_schedule_bot .

ENV BOT_TOKEN=""

CMD ["./bsu_schedule_bot"]
