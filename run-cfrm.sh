#!/usr/bin/env bash

./cfrm --game-type holem --card-abstraction cluster --card-abstraction-param ./result/buckets.all.holdem.13.4.3.re_neeo \
--runtime 2592000 --checkpoint 43200 --dump-strategy ./result/holdem.limit.2p.game.strategy \
--threads 180 --handranks ./handranks.dat --gamedef ./games/holdem.limit.2p.game