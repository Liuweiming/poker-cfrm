#!/usr/bin/env bash

./cfrm --game-type holdem \
--runtime 360000 --checkpoint 7200  \
--dump-strategy /data/liuwm/cfrm/fhp.limit.2p.game.strategy \
--card-abstraction cluster \
--card-abstraction-param ./result/is_cluster.card_abs \
--print-best-response \
--threads 180 --handranks ./handranks.dat \
--gamedef ./games/fhp.limit.2p.game
