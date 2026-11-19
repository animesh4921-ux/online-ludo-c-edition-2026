# Online Ludo World — C edition

This is a separate C-backed edition. It does not modify or deploy the existing JavaScript project. Source reference: local `online-ludo-worldwide-v1.3`.

## Reused frontend

`frontend/index.html` and `frontend/style.css` are copied from the existing project. `frontend/app.js` keeps the board and rendering while connecting through HTTP polling. `frontend/reference-app.js` preserves the original Socket.IO client for comparison. Board orientation and home lanes follow the source: Red-bottom, Green-left, Yellow-top, Blue-right.

## Included

The C11 rules engine covers four colours, two tokens per player, deployment without a six, full first-roll distance, exact finish, safe squares, captures, six/capture extra turns, finish ranks, skipping finished players and bot moves. The C HTTP server provides bot games, private rooms, public matchmaking (bots fill seats after 15 seconds), room state polling, server-side dice and moves, and bot takeover after a client has been absent for 20 seconds. It uses one process and in-memory rooms.

## Build and run on Windows

Install Visual Studio Build Tools with **Desktop development with C++**. Open **Developer Command Prompt for VS 2026** (or VS 2022), change to the `backend` folder, then compile the checks and server:

```bat
cl /nologo /W4 /std:c11 /Iinclude src\ludo.c test\test_ludo.c /Fe:ludo-test.exe
ludo-test.exe
cl /nologo /W4 /std:c11 /Iinclude src\ludo.c src\server.c /Fe:ludo_server.exe ws2_32.lib
ludo_server.exe
```

Open <http://localhost:3001>. Keep the command window open while playing. Set `PORT` to change the port. For LAN play, allow the server through Windows Firewall and use `http://YOUR-PC-LAN-IP:3001` on the other device.

## Prepare a separate Render service

This folder has its own `Dockerfile`, so it can be deployed as a separate Docker web service. Render web services need the server to bind to `0.0.0.0` and use the `PORT` environment variable; this server does both. [Render web-service guidance](https://render.com/docs/web-services).

1. Put the contents of this `c-version` folder in a separate GitHub repository. Do not replace files in the repository that backs the existing game.
2. In Render, choose **New + → Web Service**, connect that separate repository, and select **Docker** as the runtime.
3. Leave the Dockerfile path as `/Dockerfile`, then create the service. Render supplies `PORT` and the service gets its own `onrender.com` URL.
4. Open that new URL from each device. Players on different Wi-Fi networks can join using the private room code or public matchmaking.

Deployment has not been started. The current JavaScript model, Render service and URL remain untouched. This C version keeps active rooms in memory in one server process; restarting the new service ends active matches.
