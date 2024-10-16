/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

provider test_prov {
	probe place(int i, int j) : (int j, int i, int i, int j);
	probe place2(int i, char *j) : (char *j, int i, int i, char *j);
	probe place3(int i, char *j) : (char *j);
	probe place4(int i, char *j) : ();
};
