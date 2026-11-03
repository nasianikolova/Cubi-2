# Cubi 2 app

A web app to see and control Cubi 2. It shows the cube in 3D and talks to the real cube over the cube's own Wi-Fi.

## Two modes

- **Simulation** - the cube or the sphere moves on your screen, with the same commands as the real one. No cube needed.
- **Live Cubi** - the app shows the real Cubi 2 as it moves.

In both modes a **real-time chart** shows the speed of the three wheels. The top bar also opens:

- **Runs** - every run kept, as a chart of all runs and of the last one;
- **Settings** - the cube's settings;
- **Terminal** - the commands, typed one by one.

The buttons **Sphere** and **Cube** choose the shape.

## Start it

1. Install **Node.js**, version 18 or newer.
2. Open a terminal in this folder, `Code/cubi2app`.
3. Type:

   ```
   node tools/serve.mjs
   ```

4. Open **http://localhost:8000/html/index.html** in any browser.

If door 8000 is taken, the server says so; then type `node tools/serve.mjs --port 8001` and open the same page on 8001.

## Connect to the real cube

1. Turn Cubi 2 on.
2. Join the Wi-Fi **Cubi2** (password **cubi2cubi**).
3. In the app, press **Live Cubi**, type `/cube` in the Cubi card, and press **Connect**.

The server passes everything under `/cube` to the cube at `http://192.168.4.1`. Another address: `node tools/serve.mjs --cube http://<address>`.

## Where the runs go

A page in a browser cannot write a file, so the server writes each run into `data/runs.json`. Open that file to add a note of what you changed before the next run. The last run's recording goes into `data/last-run.json`. The server writes only these two files.

## What is where

```
html/    the page
js/      the app, one simple file for each function
css/     the look
fonts/   Roboto and Roboto Mono
data/    the runs, and the 3D shape of the cube
lib/     three.js, for the 3D view
tools/   small helpers written here instead of a library;
         serve.mjs is the small server
```

The code is native JavaScript (ES6 modules), with no framework, so it can be read by students who are just starting to program.

## Next

- An application for school labs, wrapped with Electron.
- Later, a Raspberry Pi with a web server, for students who love microelectronics.

## License

MIT - see `LICENSE`. The parts made by others keep their own licenses - see `THIRD-PARTY-LICENSES`.
