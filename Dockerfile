# Build CivicShield with the same C++17 source used by the existing Makefile.
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \\
    g++ make ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY Makefile ./
COPY src ./src
COPY third_party ./third_party
RUN make

# Small runtime image; no compiler is needed to run the server.
FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \\
    libstdc++6 libgcc-s1 ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/DisasterManagementServer ./DisasterManagementServer
COPY public ./public
# Keep pristine seed CSVs in the image; entrypoint copies only missing files
# into the mounted data directory, preserving existing persistent data.
COPY data ./seed-data
COPY docker-entrypoint.sh ./docker-entrypoint.sh
RUN chmod +x ./docker-entrypoint.sh && mkdir -p /data
ENV PORT=8080
ENV HOST=0.0.0.0
ENV DMS_DATA_DIR=/data
EXPOSE 8080
ENTRYPOINT ["/app/docker-entrypoint.sh"]
