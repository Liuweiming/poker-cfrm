#!/usr/bin/env bash

export OMP_NUM_THREADS=30

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 10  \
--print-best-response \
--card-abstraction cluster \
--card-abstraction-param ./result/buckets.all.holdem.13.4.3.re_neeo \
--threads 30 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
