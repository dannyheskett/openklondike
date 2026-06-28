#!/bin/bash
# Incremental build and run (header dependencies are tracked by the Makefile).
cd "$(dirname "$0")"
make run
