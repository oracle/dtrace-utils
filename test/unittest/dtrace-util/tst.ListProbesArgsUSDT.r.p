#!/bin/sed -rf
s,test_prov[0-9]*,test_provXXXX,g; s,^ *[0-9]+, XX,g;
