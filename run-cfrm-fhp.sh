#!/usr/bin/env bash

export OMP_NUM_THREADS=10

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 60  \
--print-best-response \
--card-abstraction cluster \
--card-abstraction-param ./result/is_cluster.card_abs \
--threads 10 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
