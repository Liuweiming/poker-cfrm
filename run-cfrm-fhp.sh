#!/usr/bin/env bash

export OMP_NUM_THREADS=30

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 100  \
--print-best-response \
--threads 30 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
