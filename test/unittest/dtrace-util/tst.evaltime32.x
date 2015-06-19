#!/bin/bash
exit 1 # pro tem, release branch xfail
[[ -e test/triggers/visible-constructor-32 ]] && exit 0
exit 1
