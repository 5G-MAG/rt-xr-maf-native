#!/bin/bash
BASEDIR=$(dirname $0)/..

# create a python virtual environment
python3 -m venv $BASEDIR/venv

# activate the virtual environment
# on windows, assume gitbash
if [[ "$OSTYPE" == "msys" ]]; then
    source $BASEDIR/venv/Scripts/activate
else
    # mac / linux
    source $BASEDIR/venv/bin/activate
fi
python -m pip install meson ninja