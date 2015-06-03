#!/bin/bash

rm -f irc.freenode.org/\#potager2/in
rm -f irc.freenode.org/\#potager2/out
./ii -i . -s irc.freenode.org -p 6667 -n lefuneste -f the_spartan
