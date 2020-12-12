sizeof('c') : 4
sizeof(10 * 'c') : 4
sizeof(100 + 12345) : 4
sizeof(1234567890) : 4
sizeof(1234512345 * 1234512345 * 12345678 * 1ULL) : 8
sizeof(-129) : 4
sizeof({ptr}/{ptr}) : 4
sizeof(3 > 2 ? 3 : 2) : 4

