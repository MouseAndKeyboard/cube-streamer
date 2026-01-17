FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config \
    libgstreamer1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad libgstreamer-plugins-bad1.0-dev \
    libwebsockets-dev libegl1-mesa-dev libgles2-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S server -B build && cmake --build build

EXPOSE 8080
CMD ["./build/cube_server"]
