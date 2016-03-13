#!/bin/bash

sox "${1}" -c 1 -b 8 -r 32000 "${2}"
