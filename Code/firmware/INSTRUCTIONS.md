# Cubi 2: how to start

Firmware 2026110501 (5 November 2026). The cube shows its version in its first line, and in `show`.

**Open `INSTRUCTIONS.html` in your browser.** It is the same text, with a moving picture for each part, "Bigger text", "Dark" and a "Read this" button that reads each part aloud. This file is the plain-text copy.

---

## 1. What is in the zip

Cubi has two small computers (boards). Each one gets its own program. The app comes from GitHub: github.com/nasianikolova/Cubi-2, the folder `Code/cubi2app`.

| Folder or file | What it is |
| --- | --- |
| `cubi2_brain` | The main program. It keeps Cubi balanced. It goes on the main board. |
| `cubi2_setup` | A program you run once, before the main one. It tests the motors and the sensor. |
| `cubi2_radio` | The program of the Wi-Fi board. |
| `make_update_file.py` | Makes an update file, so you can send a new program to Cubi over Wi-Fi (the Update button in the app). |

New in this version: the coach (`coach.h`). It learns from your real tries, checks the sensors and notices knocks and shaking.

## 2. Put the program on Cubi

You send the program from your computer to the board through a USB cable.

> **USB or battery. Never both at the same time.** While the cable is in, the battery stays out.

- Open the folder `cubi2_brain` in the editor.
- Press **Build**, then **Upload**.

## 3. Connect the app

Cubi makes its own Wi-Fi network. Your computer joins it, like joining a friend's hotspot.

- Wi-Fi network: **Cubi2**
- Password: **cubi2cubi**
- Address: **192.168.4.1**

- On your computer, join the Wi-Fi network **Cubi2** with the password **cubi2cubi**.
- In a terminal, in the folder `cubi2app`, type `node tools/serve.mjs` (Node.js 18 or newer). Leave it open.
- In the browser, open **localhost:8000/html/index.html**.
- At the top, choose **Live Cubi**.
- In the Cubi card, type **/cube** and press **Connect**. The app's server passes it on to the cube at 192.168.4.1.

## 4. The golden rule: Cubi always asks first

Cubi changes or saves nothing on its own. First it asks you. You answer Yes or No.

In the terminal, a question looks like this:

    ? it learned a new balance point while it balanced (corner 0.25 deg) - keep it and save? (yes / no)

In the app, the same question appears in a bar over the 3D view, with **Yes** and **No** buttons. It also shows which step of "the road to 18 minutes" it belongs to.

Questions come one at a time. If you start a new try, the open questions are dropped. After that try, Cubi asks again whatever still matters.

## 5. After every try

When a try ends (you press Stop, or Cubi falls), Cubi does three things, like a coach after a training session.

- **It tells you how the try went.** It looks at the last 4 seconds, 250 readings every second: how much it leaned, how fast it moved, and how it shook: slowly, in the middle, or fast. (The app gets only 10 readings a second, so it cannot see a fast shake. Cubi can.)
- **It looks at its balance point.** If it found a better point while balancing, it asks: keep it and save? If not, it works out the point from how the wheels pushed, and asks if it should move there.
- **It suggests at most ONE change.** One change at a time, so you always know what helped.

| What Cubi saw | What it suggests |
| --- | --- |
| A fast shake that the wheels follow | A filter that calms that shake |
| A twist on the corner's tip | Less twist |
| A wobble, a few times a second | A calmer reaction |
| A slow, big sway | A stronger reaction |

If you say **Yes**, Cubi uses the change, and saves nothing yet. After the next try it compares the two:
- **Better, or about the same:** it asks "keep it and save?"
- **Worse:** it asks "go back?"

After a long, calm try (one minute or more), Cubi offers to send its settings to the app as a profile, so you can start from them next time.

## 6. Cubi notices the world around it

Cubi can tell what it did itself from what happened around it. It checks all the time, 250 times a second.

- **A wrong reading.** One reading jumps far away, and the ones before and after it agree. Nothing real moves like that in 4 thousandths of a second. Usually a loose wire on the sensor's cable.
- **A knock.** A jump that lasts: a hand, a bump, or the table moving.
- **Shaking.** A steady fast trembling: a machine nearby, footsteps, or a loose part.

After a try, Cubi lists what it noticed. If something knocked it in the last 4 seconds, it learns nothing from that try and suggests no change. If it falls just after a knock, it tells you.

## 7. The Guide in the app

Press **Guide** at the top of the app. It takes you through 8 short steps, with a moving picture for each one.

- **Where:** just the cube, or inside the ball.
- **Floor:** silicone mat, hard table, soft foam, or something else.
- **Balance point:** every Cubi is a little different (cables, lids, battery), so first find YOUR Cubi's point: hang it from corner 0 (or the ball from its 8 navels).
- **Place:** Cubi checks itself, and you check that the floor is flat.
- **Profile:** settings that already worked (how strongly it reacts, its filters). Your Cubi keeps its own balance point.
- **Choices:** two small questions.
- **Send:** the app shows what it will send. Cubi checks every number.
- **Start.**

The Guide has "Bigger text" and "Read to me" buttons too.

## 8. Draw a route for the ball

Draw a line on the floor in the app, and the ball follows it.

- In the **Drive** card, press **Draw a route**.
- Draw on the floor with the mouse or a finger.
- Press **Go**.

The app cuts your line into straight pieces and sends them one after the other: turn left or right, then go forward. It works in the Simulation and with the real Cubi. The real ball still settles into the openings of its cage; rolling along the arcs is the next step.

## 9. Commands you can type

Open the **Terminal** in the app and type a command.

| Type | What happens |
| --- | --- |
| `yes` / `no` | Your answer to Cubi's question. |
| `go corner` | Cubi gets ready to balance on corner 0. Lift it slowly; it takes over. |
| `go edge x` (or y, z) | Balance on an edge. |
| `stop` | Stop at once. |
| `check` | Cubi checks itself: its sensor, battery and motors. Keep it still for 2 seconds. It also does this when it starts. |
| `zero hang` | Cubi hangs still from corner 0, and reads its true balance point. Then it asks to save. |
| `zero hang 3` | The ball hangs still from navel 3 (any of 0 to 7), and reads that navel's point. |
| `slope`, then `slope turn` | Is the floor flat? Lay Cubi on a side and type `slope`. Turn it half a turn on the same spot and type `slope turn`. |
| `coach on` / `coach off` | The report and the suggestion after each try. The balance point question always comes. |
| `profile` | Cubi shows its settings as one line. The app keeps it. |
| `fix` | Move the balance point to where the wheels pushed in the last try. |
| `save` | Keep the settings when Cubi is switched off. |
| `help` | The list of all commands. |

For the ball:

| Type | What happens |
| --- | --- |
| `go sphere` | The ball holds still. Then: `fwd 30`, `back 30` (cm), `left 90`, `right 90` (degrees), `halt`. |
| `go navel 3` | The ball balances on navel 3. |
| `go arc` | The ball balances on one of its arcs. Hold it upright on the arc; it takes over. |
| `roll 20` | On the arc: roll 20 degrees along it, and back (1 to 30). |

## 10. The road to 18 minutes

In two days (22 and 23 October 2026), the first Cubi went from its first wheel turn to standing 17.9 minutes on one corner. This is how we thought. Cubi's coach thinks the same way.

- **Watch.** The flight recorder shows what really happened in the last 4 seconds.
- **Change one thing,** then try again. That way you know what helped.
- **Compare fairly:** the same mode, the same floor, the same start.
- **Keep what helps,** undo the rest, and write both down.
- **Look at the whole machine.** The biggest step was in the mechanics: a solid table for the sensor.
- **Safety first:** Stop must always work.

All 18 steps, with what we saw, what we changed and what came out: in the app, **Guide → The road to 18 minutes**.

## 11. Your first test with this program

- Upload the program. When Cubi starts, keep it still and read its self-check. Answer its questions.
- Open the **Guide**: just the cube, your floor.
- Hang it from corner 0 and press **Read the level**. Answer "keep it and save?". Put the pointed screw back into the tip.
- Check the place: **Check**, then **Read 1** and **Read 2**. Then a profile, your choices, and **Send**. Answer Cubi's question.
- Type `go corner` and lift it slowly. Stop after 1-2 minutes. Read the report and answer the questions.
- Send the terminal lines and your answers. The coach's rules come from the tries of 22-23 October and have been tested only in the simulation so far.

## 12. Safety

Cubi is small, and its wheels are fast and strong.

- **Fingers and hair away from the open sides.** The wheels can turn 4000 times a minute.
- **The battery is 24 V.** When you upload a program: USB or battery, never both.
- **Stop must always work.** If it ever does not, take the battery out and write down what happened.
