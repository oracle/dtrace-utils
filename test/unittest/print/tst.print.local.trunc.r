{ptr} = *
                                            (struct test_struct) {
                                             .a = (int)1,
                                             .b = (char [5]) [
                                              (char)'t',
                                              (char)'e',
                                              (char)'s',
                                              (char)'t',
                                             ],
                                             ...
                                            }
{ptr} = *
                                            (struct test_struct) {
                                             .a = (int)1,
                                             .b = (char [5]) [
                                              (char)'t',
                                              (char)'e',
                                              (char)'s',
                                              (char)'t',
                                             ],
                                             .u = (union) {
                                              .c = (__u8)78,
                                              .d = (__u64)12345678,
                                             ...
                                             },
                                            }
