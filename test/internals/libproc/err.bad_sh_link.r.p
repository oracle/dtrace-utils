#!/usr/bin/sed -Ef
# Fix up DEBUG lines
s/DEBUG [0-9]+:/DEBUG:/
s/, [0-9]+\]/, NNN]/
s:/([^/]*/)*::g
# Rewrite pid probename
s/pid[0-9]+:/pidNNN:/g
# Drop lines of uninteresting output
/Delay in ns needed/d
