#!/usr/bin/env bash

./cfrm --game-type leduc \
--runtime 600 --checkpoint 10  --dump-strategy /data/liuwm/cfrm/leduc.limit.2p.game.strategy \
--print-best-response \
--threads 6 --handranks ./handranks.dat --gamedef ./games/leduc_4_13.limit.2p.game
