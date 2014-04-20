== DC Motor Simulator ==

This programm simulates electrical

	E = kΦ·ω

	U = i·R + L (di / dt) + E
	di / dt = (U - i·R - E

and mechanical

	Tm = kΦ·i;
	Ts = f(ω);

	J·(dω / dt) = Tm - Ts;
	ω = dφ / dt;
	dω / dt = (Tm - Ts) / J,

differential equations of a DC motor.

Programm can show current, speed, and output coefficent (PWM mode) plots and save data to file.

Some of simulation parameters can be changed:

	* PID Controller with current limit,
	* Supply voltage,
	* Moment of inertia,
	* Inductance,
	* Resistance,
	* Load type,
	* and more.

See screenshot.

== Requirements ==

Code::Blocks is required to compile this project painlessly.

Libraries used:

gsl 1.16-1 -- The GNU Scientific Library (GSL) is a modern numerical library for C and C++ programmers
gtk3 3.12.1-1 -- Object-based multi-platform GUI toolkit (v3)
plplot 5.9.11-1 -- A cross-platform software package for creating scientific plots
