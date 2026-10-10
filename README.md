# Cubi 2

**A SpaceInClass project designed by Athanasia (Nasia) Nikolova**

Cubi 2 is a self-balancing cube for the classroom. Three reaction wheels, one on
each of three faces, and the battery sit inside a printed box; the box rides in
a cage of 12 beams and a rolling sphere around it. High-school students build it
and work with it to learn physics, mathematics, programming and space on a real
machine.

Cubi 2 is designed entirely in parametric code: one model makes the whole
mechanical build, the three electronics boards, every cable and all the printed
documents. All numbers - dimensions, clearances, screws, inserts, print
settings, weights - come from tests and many real builds and prints, not only
from theory.

This repository holds the **CAD files** (STEP). The build documents and the
source code will be published later.

## Cubi 1 and Cubi 2

Cubi 2 grows out of **Cubi 1**, the first cube, which Athanasia Nikolova created
and built at a space programme in Sofia, 2026.

Cubi 1 brought the ideas that Cubi 2 keeps:

- the **open frame** - a cube made of edges only, open on every face;
- the **beams turned by 45 degrees**, like the roof beams of the old houses and
  the old Orthodox churches of Veria;
- the **reaction wheels**.

Cubi 2 adds:

- an **inner layout built around a perfect balance of the weight**, horizontally
  and vertically;
- the **gyroscope (IMU) and the battery on the vertical axis** - the main
  diagonal the cube balances on;
- **the routing of the cables**: every wire on its own road, in order, hidden
  under the boards;
- the **aesthetics that engineering must have**.

> *Cubi 1 is my love. Cubi 2 is my way.*
>
> *My way is that every beautiful thing is also functional - but not every
> functional thing is beautiful.*
>
> - Athanasia Nikolova

## SpaceInClass

SpaceInClass is an initiative by Athanasia Nikolova from Veria, Greece. It began
at the **CASSINI Space Camp 2025** in Thessaloniki, Greece, where she designed and
assembled an 8U CubeSat model and planned its mission with her team. Its aim
is to bring real space engineering into schools through *spaceLabs*, where
students build, program and operate their own hardware. Cubi 2 is one of the
projects made for the spaceLabs.

### Why

> I want to bring knowledge into schools in a different way: more fun, with
> practical work while students learn the theory. I want to bring collaboration, so
> that already at school each of us can find the field they are good at. Students do not
> need to know everything. They can specialise, build teams and work together.
>
> Many children are bored in class. They cannot concentrate, they are not
> interested. Maybe I was like that once. Maybe these children are out there.
>
> I am waiting for ideas for more projects in SpaceInClass.
>
> - Athanasia Nikolova

## The original idea

Both Cubi 1 and Cubi 2 stand on the shoulders of the **Cubli**, the cube that
can jump up and balance on a corner, created by scientists of the Institute for
Dynamic Systems and Control at ETH Zurich, Switzerland. Their work showed that a
cube with three reaction wheels can stand up by itself and balance on an edge or
a corner. Cubi takes that idea into the classroom: printable, built by students,
and designed with its own architecture.

- M. Gajamohan, M. Merz, I. Thommen, R. D'Andrea, *"The Cubli: A Cube That Can
  Jump up and Balance"*, IEEE/RSJ International Conference on Intelligent Robots
  and Systems (IROS), 2012, pp. 3722-3727.
- M. Gajamohan, M. Muehlebach, T. Widmer, R. D'Andrea, *"The Cubli: A Reaction
  Wheel Based 3D Inverted Pendulum"*, European Control Conference (ECC), 2013.

Cubi 2 is an independent project: it is not affiliated with or endorsed by the
original authors or ETH Zurich.

## A different direction: the sphere

The Cubli stands up by braking its wheels suddenly. **Cubi has no brakes.**

The first big change is the **sphere**. Cubi changes the original purpose: it
aims at **motion**, not only at standing up and balancing. The sphere is the
perfect shape, and it is where I intend to experiment the most. **Cubi 2 has
the sphere.**

## What I intend

This is a statement of intentions, not of results: I announce no discovery.

- I will **experiment** with the real cube and run **simulations** of it.
- The aim is to find out whether these ideas can be **useful in space
  applications**.
- Along the way, the research is meant to be a mix of **play, tests, failures,
  new knowledge from theory** - and perhaps a few successes.
- The project is planned to keep **students and STEM clubs** busy for **one
  academic year**.
- **SpaceInClass** will feature the good ideas that come out of this work on its
  page.

---

## What is where

```
CAD/
  print/       what the students PRINT - box, cage corners and beams, cover,
               reaction wheel, sphere arcs and connector, battery mounts,
               magnet carrier, cable clamp, standoffs (STEP, as printed)
  hardware/    what is BOUGHT and BUILT - motors, motor drivers, IMU, MCU board,
               battery connectors, spacers, and the three electronics boards
               that the students solder (STEP)
               SOURCES.txt = where each third-party model comes from + its licence
```

The full assemblies (the whole cube, and the cube with all its cables) are
larger than GitHub allows for one file (over 100 MB); they will be shared
separately.

## How the documents are made

The build documents (parts lists and step-by-step guides) are made with the help
of **artificial intelligence**. Special care went into the scientific side of
teaching with them:

- a **colour code** that stays the same in every document - each board, wire and
  insert size has its own colour, chosen to be told apart also by students with
  low vision or colour-blindness;
- **images and diagrams** drawn from the real 3D model, with real colours, so
  that what students see on paper is what they hold in their hands;
- large print and short steps, so that every student can follow them.

The aim is simple: to make the engineering **easier to understand** for
students.

## Design rules

- The **printed parts** in `CAD/print/` are the parts as printed; the parts in
  `CAD/hardware/` keep their original colours.
- The **cage and the box are fixed**: they are already printed, and no later
  change may move them, not even by 0.001 mm.
- **Cables** run only on the cable roads: under the boards, along the three
  edges at corner 0, and on the sides of the corner-0 triangle. Power wires lie
  under the signal wires; no wire crosses another on a board; every wire gets
  under a board across a side, between that side's two standoffs.
- **Documents** are in large print, with real colours, readable by low-vision
  students.
- Everything is in English.

## Units and frames

Millimetres. Corner 0 of the box is the origin; the three motor faces are x, y
and z, with the wheels towards -x, -y and -z.

## Licence

Cubi 2 is **free for everyone**, as long as the name **Cubi 2** and its authors
are credited.

- **Code** (published later, and any code added to the project): **MIT** - see `LICENSE`.
- **CAD, drawings, documents, images**: **CC BY 4.0** - see `LICENSE-CAD.md`.
  Credit line: *Based on Cubi 2 by Athanasia Nikolova / SpaceInClass, CC BY 4.0.*
- **Third-party models** in `CAD/hardware/` keep their own licences - see
  `CAD/hardware/SOURCES.txt`.

Cubi 2 is made for high-school students working **under the supervision of a
teacher or mentor**. Please read [`RESPECT.md`](RESPECT.md) - how the project asks
to be treated, and the safety note (battery, spinning wheels, soldering).

## Credits

Design: Athanasia Nikolova, for SpaceInClass.
