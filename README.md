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