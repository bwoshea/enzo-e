#!/bin/bash

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

hg diff | $dir/_diff-org.awk > diff.org

