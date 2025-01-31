#!/bin/bash
BASEDIR=$(dirname $0)/..
python3 -m venv $BASEDIR/venv
source $BASEDIR/venv/bin/activate
python3 -m pip install meson ninja