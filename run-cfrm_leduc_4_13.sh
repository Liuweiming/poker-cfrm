#!/usr/bin/env bash

export OMP_NUM_THREADS=20

./cfrm --game-type leduc \
--runtime 600 --checkpoint 10  --dump-strategy /data/liuwm/cfrm/leduc.limit.2p.game.strategy \
--print-best-response \
--threads 1 --handranks ./handranks.dat --gamedef ./games/leduc_4_13.limit.2p.game
