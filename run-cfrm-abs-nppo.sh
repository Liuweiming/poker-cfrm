#!/usr/bin/env bash

./cfrm --game-type holem --card-abstraction cluster --card-abstraction-param /data/liuwm/cfrm/poker-cfrm/nppo_cluster_5000_5000_5000.card_abs \
--runtime 2592000 --checkpoint 43200 --dump-strategy /data/liuwm/cfrm/holdem.limit.2p.game.nppo_day.strategy \
--threads 1 --handranks ./handranks.dat --gamedef ./games/holdem.limit.2p.game
