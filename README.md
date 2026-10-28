# PocketPet

A gentle virtual pet for the **M5StickC Plus2**, made for kids.

The pet **never dies**. If nobody looks after it, it gets hungry, bored or
sleepy and asks for help. As soon as someone feeds it or plays with it, it
cheers up again.

![All the screens](docs/screenshots/all-screens.png)

## What you need

| Item | About |
|---|---|
| M5StickC Plus2 (one per child) | $20–30 depending on the store |
| USB-C cable that carries data, not just charging | probably already have one |
| A computer (Windows, Mac or Linux) | for the one-time setup |

The stick already has the screen, buttons, speaker, rechargeable battery and a
clock built in. No soldering is needed.

## Putting the game on the stick (one time, about 20 minutes)

1. Install the **Arduino IDE** from <https://www.arduino.cc/en/software>.
2. Add M5Stack's boards:
   - Open **File → Preferences** (on a Mac: **Arduino IDE → Settings**).
   - In **Additional boards manager URLs**, paste:
     `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
   - Open **Tools → Board → Boards Manager**, search for **M5Stack**, and
     click **Install**.
3. Add the M5 library: open **Tools → Manage Libraries**, search for
   **M5Unified**, and click **Install**. Say yes if it asks to install
   other libraries it needs.
4. Download this project (the green **Code** button on GitHub, then
   **Download ZIP**) and unzip it.
5. In the Arduino IDE, use **File → Open** to open `PocketPet/PocketPet.ino`.
6. Plug the stick in. Choose **Tools → Board → M5Stack → M5StickCPlus2**,
   then pick its port under **Tools → Port**.
7. Click the **Upload** arrow (→). When it says "Done uploading", the egg
   appears.

Do steps 6–7 again for the second stick.

> If the upload can't find the stick, try another cable. Many cables only
> charge. On Windows you may also need the CH9102 USB driver from M5Stack's
> website.

## How to play

Hold the stick upright with the screen facing you:

| Button | Where | What it does |
|---|---|---|
| **A** | the big button under the screen | do it / choose |
| **B** | the button on the right side | next |
| **PWR** | the button on the left side | go back. On the home screen it switches off. Press it to switch on again. |

**First time:** press A three times to hatch the egg. Then pick a color, pick
ears (cat, bunny, bear or a little sprout), spell a name, and set the clock.

**Every day:** press B to move along the bottom bar and A to choose:

- **Feed**: a *Meal* fills the tummy; a *Treat* is a little food and a lot of
  fun. It says "I'm full!" if it isn't hungry.
- **Play**: the "Which way?" game. Guess which way the pet will look (A = left,
  B = right). Five rounds.
- **Clean**: sweeps up poop. Poop makes the pet a bit sad until it's gone.
- **Lights**: puts it to bed, or wakes it up.
- **Cuddle**: hearts!
- **How am I?**: food, fun and energy, its age, and how many hearts it has
  collected.
- **Settings**: see below.

When the pet needs something, a thought bubble appears over its head: an apple
(hungry), a ball (bored), a moon (sleepy) or a broom (messy). Every so often it
also plays a little tune.

It grows from **baby** to **kid** after one day, and to **grown-up** (with a
bow) after three days.

## Settings (for grown-ups)

| Setting | What it does |
|---|---|
| **School mode** | Freezes the pet completely, including while the stick is off. Turn it on before school and off after. |
| **Sound** | Turns the beeps on or off. |
| **Brightness** | Low, mid or high. Lower lasts longer on battery. |
| **Auto off** | Switches off after 1, 2 or 5 minutes without a button press, to save battery. The screen dims after 20 seconds. |
| **Bedtime** | On (the default): the pet falls asleep by itself at 8 pm and wakes at 7 am. If the kids wake it at night, it stays up a couple of minutes and then goes back to sleep. |
| **Set clock** | 24-hour time. You'll also be asked again if the clock ever loses power. |
| **New egg** | Starts over. You have to hold A for 3 seconds so it can't happen by accident. |

## How "never dies" works

- **Needs go down slowly.** A full tummy lasts about 10 hours, and fun about
  8 hours.
- **Running out of energy just means a nap.**
- **Switched off, time still passes.** The stick's clock keeps running, but the
  pet's needs drop at half speed and never below 20%. After a weekend away it's
  hungry and pleased to see you, never miserable.
- **Saving is automatic.** The pet is saved every 30 seconds and when the
  stick switches off, so a flat battery doesn't lose it.

You can change any of these numbers in `PocketPet/config.h`. For example, raise
`OFFLINE_FLOOR` to make it even gentler, or change `BEDTIME_HOUR`.

## Battery

The M5StickC Plus2 has a small battery: a few hours with the screen on, much
longer when it's off. That's why it switches itself off. Charge it with any
USB-C charger.

## For tinkerers

- `PocketPet/pet.cpp`: the rules (no hardware code).
- `PocketPet/ui.cpp`: all the drawing. The pet is drawn from shapes, not
  pictures, so colors and ears are easy to add.
- `PocketPet/PocketPet.ino`: buttons, sound, clock, saving and power.
- `PocketPet/config.h`: all the numbers that set how fast things happen.
- `tools/host/`: tests that run on a computer without the stick:
  - `make -C tools/host test` checks the rules.
  - `make -C tools/host screenshots M5GFX=<path to an M5GFX checkout>`
    draws the screens.
  - `make -C tools/host sim M5GFX=<path to an M5GFX checkout>` runs the
    real `PocketPet.ino` through a pretend play session.

![A simulated play session](docs/screenshots/play-session.png)

PocketPet started from [TamaFi](https://github.com/cifertech/TamaFi) by
CiferTech; its meadow background art is used here under the MIT license (see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
