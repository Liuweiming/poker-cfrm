#!/usr/bin/env bash

./cfrm --game-type leduc \
--runtime 600 --checkpoint 10  --dump-strategy /data/liuwm/cfrm/leduc.limit.2p.game.strategy \
--print-best-response --print-abstract-best-response \
--threads 2 --handranks ./handranks.dat --gamedef ./games/leduc.limit.2p.game
