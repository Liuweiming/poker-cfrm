#!/usr/bin/env bash

./cfrm --game-type holem --card-abstraction cluster --card-abstraction-param ./nsss_cluster_large.card_abs \
--runtime 2592000 --checkpoint 86400 --dump-strategy /data/liuwm/cfrm/holdem.limit.2p.game.nsss_day.strategy \
--threads 90 --handranks ./handranks.dat --gamedef ./games/holdem.limit.2p.game
