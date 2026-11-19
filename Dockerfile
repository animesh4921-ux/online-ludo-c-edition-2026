FROM gcc:14-bookworm AS build
WORKDIR /src
COPY backend/include ./backend/include
COPY backend/src ./backend/src
RUN gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Ibackend/include \
    backend/src/ludo.c backend/src/server.c -o /ludo_server

FROM debian:bookworm-slim
WORKDIR /app/backend
COPY --from=build /ludo_server ./ludo_server
COPY frontend /app/frontend
ENV PORT=10000
EXPOSE 10000
CMD ["./ludo_server"]
