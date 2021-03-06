#!/usr/bin/perl

BEGIN {
	if (substr("foobarbazbop", 3) ne "barbazbop") {
		printf("perl => substr(\"foobarbazbop\", 3) = \"%s\"\n",
		    substr("foobarbazbop", 3));
		printf("   D => substr(\"foobarbazbop\", 3) = \"%s\"\n",
		    "barbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 300) ne "") {
		printf("perl => substr(\"foobarbazbop\", 300) = \"%s\"\n",
		    substr("foobarbazbop", 300));
		printf("   D => substr(\"foobarbazbop\", 300) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", -10) ne "obarbazbop") {
		printf("perl => substr(\"foobarbazbop\", -10) = \"%s\"\n",
		    substr("foobarbazbop", -10));
		printf("   D => substr(\"foobarbazbop\", -10) = \"%s\"\n",
		    "obarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 0) ne "foobarbazbop") {
		printf("perl => substr(\"foobarbazbop\", 0) = \"%s\"\n",
		    substr("foobarbazbop", 0));
		printf("   D => substr(\"foobarbazbop\", 0) = \"%s\"\n",
		    "foobarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 1) ne "oobarbazbop") {
		printf("perl => substr(\"foobarbazbop\", 1) = \"%s\"\n",
		    substr("foobarbazbop", 1));
		printf("   D => substr(\"foobarbazbop\", 1) = \"%s\"\n",
		    "oobarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 11) ne "p") {
		printf("perl => substr(\"foobarbazbop\", 11) = \"%s\"\n",
		    substr("foobarbazbop", 11));
		printf("   D => substr(\"foobarbazbop\", 11) = \"%s\"\n",
		    "p");
		$failed++;
	}

	if (substr("foobarbazbop", 12) ne "") {
		printf("perl => substr(\"foobarbazbop\", 12) = \"%s\"\n",
		    substr("foobarbazbop", 12));
		printf("   D => substr(\"foobarbazbop\", 12) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 13) ne "") {
		printf("perl => substr(\"foobarbazbop\", 13) = \"%s\"\n",
		    substr("foobarbazbop", 13));
		printf("   D => substr(\"foobarbazbop\", 13) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 8, 20) ne "zbop") {
		printf("perl => substr(\"foobarbazbop\", 8, 20) = \"%s\"\n",
		    substr("foobarbazbop", 8, 20));
		printf("   D => substr(\"foobarbazbop\", 8, 20) = \"%s\"\n",
		    "zbop");
		$failed++;
	}

	if (substr("foobarbazbop", 4, 4) ne "arba") {
		printf("perl => substr(\"foobarbazbop\", 4, 4) = \"%s\"\n",
		    substr("foobarbazbop", 4, 4));
		printf("   D => substr(\"foobarbazbop\", 4, 4) = \"%s\"\n",
		    "arba");
		$failed++;
	}

	if (substr("foobarbazbop", 5, 8) ne "rbazbop") {
		printf("perl => substr(\"foobarbazbop\", 5, 8) = \"%s\"\n",
		    substr("foobarbazbop", 5, 8));
		printf("   D => substr(\"foobarbazbop\", 5, 8) = \"%s\"\n",
		    "rbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 5, 9) ne "rbazbop") {
		printf("perl => substr(\"foobarbazbop\", 5, 9) = \"%s\"\n",
		    substr("foobarbazbop", 5, 9));
		printf("   D => substr(\"foobarbazbop\", 5, 9) = \"%s\"\n",
		    "rbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", 400, 20) ne "") {
		printf("perl => substr(\"foobarbazbop\", 400, 20) = \"%s\"\n",
		    substr("foobarbazbop", 400, 20));
		printf("   D => substr(\"foobarbazbop\", 400, 20) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 400, 0) ne "") {
		printf("perl => substr(\"foobarbazbop\", 400, 0) = \"%s\"\n",
		    substr("foobarbazbop", 400, 0));
		printf("   D => substr(\"foobarbazbop\", 400, 0) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 400, -1) ne "") {
		printf("perl => substr(\"foobarbazbop\", 400, -1) = \"%s\"\n",
		    substr("foobarbazbop", 400, -1));
		printf("   D => substr(\"foobarbazbop\", 400, -1) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 3, 0) ne "") {
		printf("perl => substr(\"foobarbazbop\", 3, 0) = \"%s\"\n",
		    substr("foobarbazbop", 3, 0));
		printf("   D => substr(\"foobarbazbop\", 3, 0) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", 3, -1) ne "barbazbo") {
		printf("perl => substr(\"foobarbazbop\", 3, -1) = \"%s\"\n",
		    substr("foobarbazbop", 3, -1));
		printf("   D => substr(\"foobarbazbop\", 3, -1) = \"%s\"\n",
		    "barbazbo");
		$failed++;
	}

	if (substr("foobarbazbop", 3, -4) ne "barba") {
		printf("perl => substr(\"foobarbazbop\", 3, -4) = \"%s\"\n",
		    substr("foobarbazbop", 3, -4));
		printf("   D => substr(\"foobarbazbop\", 3, -4) = \"%s\"\n",
		    "barba");
		$failed++;
	}

	if (substr("foobarbazbop", 3, -20) ne "") {
		printf("perl => substr(\"foobarbazbop\", 3, -20) = \"%s\"\n",
		    substr("foobarbazbop", 3, -20));
		printf("   D => substr(\"foobarbazbop\", 3, -20) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", -10, -5) ne "obarb") {
		printf("perl => substr(\"foobarbazbop\", -10, -5) = \"%s\"\n",
		    substr("foobarbazbop", -10, -5));
		printf("   D => substr(\"foobarbazbop\", -10, -5) = \"%s\"\n",
		    "obarb");
		$failed++;
	}

	if (substr("foobarbazbop", 0, 400) ne "foobarbazbop") {
		printf("perl => substr(\"foobarbazbop\", 0, 400) = \"%s\"\n",
		    substr("foobarbazbop", 0, 400));
		printf("   D => substr(\"foobarbazbop\", 0, 400) = \"%s\"\n",
		    "foobarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", -1, 400) ne "p") {
		printf("perl => substr(\"foobarbazbop\", -1, 400) = \"%s\"\n",
		    substr("foobarbazbop", -1, 400));
		printf("   D => substr(\"foobarbazbop\", -1, 400) = \"%s\"\n",
		    "p");
		$failed++;
	}

	if (substr("foobarbazbop", -1, 0) ne "") {
		printf("perl => substr(\"foobarbazbop\", -1, 0) = \"%s\"\n",
		    substr("foobarbazbop", -1, 0));
		printf("   D => substr(\"foobarbazbop\", -1, 0) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", -1, -1) ne "") {
		printf("perl => substr(\"foobarbazbop\", -1, -1) = \"%s\"\n",
		    substr("foobarbazbop", -1, -1));
		printf("   D => substr(\"foobarbazbop\", -1, -1) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", -24, 24) ne "foobarbazbop") {
		printf("perl => substr(\"foobarbazbop\", -24, 24) = \"%s\"\n",
		    substr("foobarbazbop", -24, 24));
		printf("   D => substr(\"foobarbazbop\", -24, 24) = \"%s\"\n",
		    "foobarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", -24, 12) ne "") {
		printf("perl => substr(\"foobarbazbop\", -24, 12) = \"%s\"\n",
		    substr("foobarbazbop", -24, 12));
		printf("   D => substr(\"foobarbazbop\", -24, 12) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("foobarbazbop", -24, 13) ne "f") {
		printf("perl => substr(\"foobarbazbop\", -24, 13) = \"%s\"\n",
		    substr("foobarbazbop", -24, 13));
		printf("   D => substr(\"foobarbazbop\", -24, 13) = \"%s\"\n",
		    "f");
		$failed++;
	}

	if (substr("foobarbazbop", -12, 12) ne "foobarbazbop") {
		printf("perl => substr(\"foobarbazbop\", -12, 12) = \"%s\"\n",
		    substr("foobarbazbop", -12, 12));
		printf("   D => substr(\"foobarbazbop\", -12, 12) = \"%s\"\n",
		    "foobarbazbop");
		$failed++;
	}

	if (substr("foobarbazbop", -12, 11) ne "foobarbazbo") {
		printf("perl => substr(\"foobarbazbop\", -12, 11) = \"%s\"\n",
		    substr("foobarbazbop", -12, 11));
		printf("   D => substr(\"foobarbazbop\", -12, 11) = \"%s\"\n",
		    "foobarbazbo");
		$failed++;
	}

	if (substr("CRAIG: Positioned them, I don't know... I'm fairly wide guy.", 100, 10) ne "") {
		printf("perl => substr(\"CRAIG: Positioned them, I don't know... I'm fairly wide guy.\", 100, 10) = \"%s\"\n",
		    substr("CRAIG: Positioned them, I don't know... I'm fairly wide guy.", 100, 10));
		printf("   D => substr(\"CRAIG: Positioned them, I don't know... I'm fairly wide guy.\", 100, 10) = \"%s\"\n",
		    "");
		$failed++;
	}

	if (substr("CRAIG: Positioned them, I don't know... I'm fairly wide guy.", 100) ne "") {
		printf("perl => substr(\"CRAIG: Positioned them, I don't know... I'm fairly wide guy.\", 100) = \"%s\"\n",
		    substr("CRAIG: Positioned them, I don't know... I'm fairly wide guy.", 100));
		printf("   D => substr(\"CRAIG: Positioned them, I don't know... I'm fairly wide guy.\", 100) = \"%s\"\n",
		    "");
		$failed++;
	}

	exit($failed);
}

