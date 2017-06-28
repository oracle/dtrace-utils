#!/bin/sed -nrf
s,test_prov[0-9]*,test_provXXXX,g; s,^ *[0-9]+, XX,g; /^ XX.*(test_provXXXX|syscall|profile)/p; /dtrace|stderr|PROVIDER/p
