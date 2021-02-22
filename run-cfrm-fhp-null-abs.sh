#!/usr/bin/env bash

export OMP_NUM_THREADS=50

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 3600  \
--print-best-response \
--card-abstraction cluster \
--card-abstraction-param ./result/is_cluster.card_abs \
--threads 50 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
