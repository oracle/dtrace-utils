#!/bin/sh
grep -vE '^$|data bytes|bytes from|ping statistics|packets transmitted|^rtt min'
