#!/bin/bash

# Wrap to ckbuild.sh
export WP=${WP:-$(realpath $PWD/../)}

bash kernel_build/ckbuild.sh "$@"
