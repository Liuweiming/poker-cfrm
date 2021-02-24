#!/usr/bin/env bash

export OMP_NUM_THREADS=100

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 14400 \
--card-abstraction cluster \
--card-abstraction-param ./result/buckets.all.holdem.13.4.3.re_neeo \
--dump-strategy ./result/holdem.limit.2p.game_neeo_re.strategy \
--threads 100 --handranks ./handranks.dat \
--gamedef ./games/holdem.limit.2p.game
