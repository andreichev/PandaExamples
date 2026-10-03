# PhysicsTest

A test project for the physics of [Panda](https://github.com/andreichev/Panda). Worlds in `Assets/`:

- `world.world` — bodies and a capsule character (`PlayerController`).
- `Joints2D.world`, `Joints3D.world` — the joints: a chain, a pendulum, a motor wheel, a spinner.
- `Vehicle.world` — the vehicle track: six lanes of obstacles and two cars built on two different
  models. Most of this file is about it.

The scripts are in `Scripts/PhysicsTest/src`; the vehicle ones in `Vehicle/`. Build them from the
editor's CMake panel or with `Scripts/PhysicsTest/scripts/configure_scripts.sh` and
`build_scripts.sh` (`PANDA_SDK_DIR` must point at `<PandaSDK>/lib/cmake/PandaSDK`).

## The track

Open `Vehicle.world` and press Play. The camera follows the car you drive; the HUD shows the speed
and the car's name.

| Key | Action |
|---|---|
| W / S, Up / Down | throttle; brake, then reverse |
| A / D, Left / Right | steering |
| Space | handbrake (locks the rear wheels) |
| R | put the car back on its wheels |
| T | back to the start |
| V | drive the other car |

The cars start side by side and look down the track. The lanes, from left to right:

| Lane | What is there | What it tests |
|---|---|---|
| A | speed bumps 5–20 cm, a washboard, offset bumps, boulders | suspension travel and damping, body roll |
| B | curbs 5–30 cm, a stair, an exit ramp | how a wheel takes an edge |
| C | hills of 10°, 20°, 30°, 40° | grip on a slope, holding the car on a slope |
| D | a 12° kicker, a gap with an 18° take-off, a tabletop | flight and landing |
| E | a wall of boxes, slalom posts, a seesaw, a pendulum, a hanging bridge | pushing and standing on moving bodies |
| F | an ice patch (friction 0.02), side slopes of 15° and 25° | low grip, cornering on a bank |

Every field of both cars can be changed in the inspector while driving: the value reaches the
running script at once. Stop brings the values back; Save (Cmd+S) during Play saves the world being
edited, not the one that plays. A value you want to keep is typed in again after Stop.

## The two cars

Both cars have the same chassis (a box collider, 905 kg), the same drive, brakes, steering and
ride settings, and the same driver: `Driving.hpp` turns the keys into a drive of every wheel (a
speed target with a torque limit, the way a motor works) and into the angles of the front wheels
(the steering wheel turns at a rate, the lock shrinks with speed to the lateral acceleration
`steerGrip`, the inner wheel turns more). The models differ only in how a wheel is held on the
car and how it grips the ground.

### Joint car — physical wheels (`JointCar`)

The chassis and the four wheels are five rigid bodies. A wheel is a sphere collider on a Wheel
Joint 3D connected to the chassis: the joint holds the wheel on the suspension axis with a spring,
limits the travel, turns the front wheels for steering and spins the wheel with a motor. The
physics engine does the whole simulation — contacts, friction, the spring, the motor. The script
only tells the joints what the driver wants: a spin target with a torque limit for every wheel and
a steering angle for the front ones. The ride frequency and damping of the body are converted
into the joint values (the joint spring is tuned over the reduced mass of the wheel and the
chassis).

What follows from the model:

- Wheels are real objects. They roll over edges, climb curbs by touching them, push boxes and are
  pushed back, press on the bridge planks with their weight and get stuck in a gap — everything is
  two-way and emergent.
- The tire is the contact friction of a sphere: one coefficient for every direction, no slip
  curve. Lateral and longitudinal grip cannot be tuned apart; the handbrake slide and the
  wheelspin come from the solver's Coulomb friction.
- Four extra bodies, four joints and their contacts per car; the solver's limits apply (box3d caps
  the angular speed of a body unless `allowFastRotation` is set, which the engine does for wheel
  joint bodies). The center of mass follows the colliders.

### Raycast car — rays instead of wheels (`RaycastCar`)

One body: the chassis. The wheels are drawn children of it without colliders. Under every wheel
the script casts rays down the suspension axis from above the wheel (`wheelRays`, spread around
the rim so that the wheel acts as a circle rather than a point) and computes everything itself at
the point where the wheel bottom meets the ground:

- the spring (from `rideFrequency`, over a quarter of the car) as a force along the ground normal,
  and on a slope the part of the weight that pulls the car along the ground, fed forward to the
  tires in shares of their loads, so a braked car stands on a hill instead of creeping;
- the damper and the tire as impulses, found in a few Gauss–Seidel passes over the wheels with
  the inverse mass the chassis shows at each wheel (`Rigidbody3DComponentAPI::getMassData`). The
  damper is implicit (it answers to the velocity it leaves), the tire is a motor along the
  rolling direction and no slip across it, both inside the friction ellipse of `gripForward` and
  `gripSide`, scaled by the square root of the surface friction — the same mixing law the engine
  uses for two colliders, so both cars see the ice the same way. A brake stronger than the grip
  locks the wheel, and a locked tire resists its slide in any direction.

The impulses are summed and applied to the chassis once per frame. What the wheel stands on gets
the opposite forces (a bridge sags), but not the impulses: sized by the chassis alone, they would
throw a light body away.

What follows from the model:

- Everything is a number in the script: grip per direction, travel, how round the wheel is. A
  tire model with slip curves would be a few more lines in `solveContact`.
- A ray sees only what is straight below it. The wheel has no width and no side: an obstacle
  lower than the chassis clearance does not stop the car, a curb lifts the wheel instead of
  blocking it, and the wheel never pushes anything sideways. The chassis collider is the only
  thing that collides.
- Cheap: one body, a dozen rays. Steady at any frame rate, but the forces are computed once per
  frame with the frame's step, so a run is not exactly the same at two frame rates until the
  physics gets a fixed step.

### How they compare

| | Joint car | Raycast car |
|---|---|---|
| Bodies per car | 5 bodies, 4 joints | 1 body |
| Who simulates the wheel | the physics engine | the script |
| Wheel contact | the real sphere: edges, sides, width | a point under the wheel, sampled as a circle |
| Grip | contact friction, one coefficient | friction ellipse, two coefficients; easy to extend |
| Interaction with objects | two-way, by contact | the ground gets the wheel load, nothing else |
| Suspension tuning | ride frequency and damping, converted for the joint | ride frequency and damping, directly |
| Where it breaks | a wheel wedged in a gap, joint stress at high speed | a ray that misses, a curb that snaps the wheel up |
| Cost | the solver carries the wheels | a dozen rays and some arithmetic |

### What games use

Rays (or shape sweeps) under the wheels are the standard way to make a car in a game. The wheel
colliders of Unity cast a ray down from the wheel center, Godot's vehicle wheels are a port of
Bullet's `btRaycastVehicle`, Unreal's Chaos vehicles trace rays or sweep shapes for their wheels.
Arcade racers, open-world cars and most driving games are built on this family; simulators stay
in it too, with far more elaborate tire models on top and shapes swept instead of rays. The
raycast car here is a minimal member of the family: a friction ellipse instead of slip curves.

Physical wheels on joints are the choice when the wheel has to be a real object: building and
sandbox games where a vehicle is assembled from parts (Besiege, Trailmakers, Scrap Mechanic and
the like), physics toys, rovers and robots in simulations. The price is the solver: more bodies,
joints under load, tuning limited to what the engine exposes. A third family, soft-body cars
(BeamNG.drive), is outside this comparison.

## Scripts

| File | Role |
|---|---|
| `Vehicle/Driving.hpp` | the driver shared by the models: pedals → wheel drive, steering wheel → wheel angles |
| `Vehicle/DriverInput.hpp` | the keyboard |
| `Vehicle/JointCar.*` | the car on wheel joints |
| `Vehicle/RaycastCar.*` | the car on rays |
| `Vehicle/Garage.*` | the vehicles of the world and the one the player drives |
| `Vehicle/ChaseCamera.*` | the camera behind the active vehicle; its `target` is the car to start in; V switches |
| `Vehicle/VehicleHud.*` | speed, vehicle name, controls |
| `Vehicle/VehicleMath.hpp` | Bamboo ↔ glm and small helpers |
