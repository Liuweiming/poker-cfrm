#!/usr/bin/env bash

export OMP_NUM_THREADS=180

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 3600  \
--dump-strategy /data/liuwm/cfrm/fhp_2_13.limit.2p.game.strategy \
--print-best-response \
--card-abstraction cluster \
--card-abstraction-param ./result/is_cluster.card_abs \
--threads 180 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
