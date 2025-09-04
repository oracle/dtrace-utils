/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Enumerations should support declaration with a trailing comma for
 *	      the last enumeration value as well as non-comma case.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 * NOTES:
 *
 */

#pragma D option quiet

enum colors {
	RED = 1,
	GREEN = 2,
	BLUE = 3,
};

enum weather {
	CLOUDY,
	SUNNY
};

enum {
	CIRCLE,
	SQUARE,
	TRIANGLE,
};

enum {
	HEXAGON,
	OCTAGON
};

typedef enum {
	RIGHT,
	LEFT,
} horizontal_t;

typedef enum {
	TOP,
	BOTTOM
} vertical_t;

typedef enum motor_transport {
	CAR,
	MOTORBIKE,
} motor_transport_t;

typedef enum nonmotor_transport {
	BIKE,
	SKATEBOARD
} nonmotor_trasport_t;

BEGIN
{
	exit(0);
}
