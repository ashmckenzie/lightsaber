#!/bin/bash

sox "${1}" --norm -e unsigned-integer -c 1 -b 8 -r 31250 "${2}"
