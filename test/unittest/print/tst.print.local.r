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
                                              .c = (__u8)21,
                                              .d = (__u64)123456789,
                                             },
                                             .s = (struct) {
                                              .e = (void *){ptr},
                                              .f = (uint64_t)987654321,
                                             },
                                             .g = (bool)1,
                                             .h = (enum eh)SECONDVAL,
                                            }

